param([Parameter(Mandatory=$true)][string]$Path)
$ErrorActionPreference = 'Stop'
$binaryPath = [IO.Path]::GetFullPath($Path)
$bytes = [IO.File]::ReadAllBytes($binaryPath)
if ($bytes.Length -lt 64 -or $bytes[0] -ne 77 -or $bytes[1] -ne 90) { throw 'Not a PE executable' }
$peOffset = [BitConverter]::ToInt32($bytes, 60)
if ($peOffset -lt 64 -or $peOffset + 24 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes, $peOffset) -ne 17744) { throw 'Invalid PE header' }
$sectionCount = [BitConverter]::ToUInt16($bytes, $peOffset + 6)
$optionalSize = [BitConverter]::ToUInt16($bytes, $peOffset + 20)
$optionalOffset = $peOffset + 24
$magic = [BitConverter]::ToUInt16($bytes, $optionalOffset)
$directoryOffset = if ($magic -eq 523) { 112 } elseif ($magic -eq 267) { 96 } else { throw 'Unknown PE format' }
if ($optionalSize -lt $directoryOffset + 40) { throw 'Invalid optional header' }
if ([BitConverter]::ToUInt32($bytes, $optionalOffset + $directoryOffset + 32) -ne 0) { throw 'Refusing to modify a signed executable' }
$sectionTable = $optionalOffset + $optionalSize
if ($sectionTable + $sectionCount * 40 -gt $bytes.Length) { throw 'Invalid section table' }
$options = [Text.RegularExpressions.RegexOptions]::IgnoreCase
$sourcePattern = [regex]::new('(?<![A-Za-z0-9])(?:[A-Za-z]:[/\\](?![/\\])|/(?:Users|home)/)[^\x00-\x1f\x7f"<>|*?]*?\.(?:cpp|cc|cxx|c|hpp|h|hxx|inl|ipp)(?=[\x00\s():]|$)', $options)
$metadataPattern = [regex]::new('(?:^|[\x00\r\n])([ \t]*(?:CMake build tool|C\+\+ Compiler|C Compiler|Python \(for build\)|Install to):[ \t]*)([^\x00\r\n]+)', $options)
$encodings = @([Text.Encoding]::GetEncoding(28591), [Text.Encoding]::Unicode)
$patches = New-Object 'System.Collections.Generic.List[object]'
for ($sectionIndex = 0; $sectionIndex -lt $sectionCount; $sectionIndex++) {
    $entry = $sectionTable + $sectionIndex * 40
    $name = [Text.Encoding]::ASCII.GetString($bytes, $entry, 8).TrimEnd([char]0)
    if ($name -notin @('.rdata', '.data', '.debug')) { continue }
    $size = [BitConverter]::ToUInt32($bytes, $entry + 16)
    $offset = [BitConverter]::ToUInt32($bytes, $entry + 20)
    $flags = [BitConverter]::ToUInt32($bytes, $entry + 36)
    if (($flags -band 0x20000000) -ne 0) { throw 'Diagnostic section contains executable code' }
    if ([long]$offset + $size -gt $bytes.Length) { throw 'Section lies outside the file' }
    foreach ($encoding in $encodings) {
        $unitSize = if ($encoding.CodePage -eq 1200) { 2 } else { 1 }
        $text = $encoding.GetString($bytes, $offset, $size)
        foreach ($match in $sourcePattern.Matches($text)) {
            $leaf = ($match.Value -split '[/\\]')[-1]
            $replacement = $leaf.PadRight($match.Length)
            $patches.Add(@{ Offset = $offset + $match.Index * $unitSize; Original = $encoding.GetBytes($match.Value); Replacement = $encoding.GetBytes($replacement) })
        }
        foreach ($match in $metadataPattern.Matches($text)) {
            $value = $match.Groups[2]
            if ($value.Value -notmatch '(?:[A-Za-z]:[/\\](?![/\\])|/(?:Users|home)/)') { continue }
            $replacement = '[omitted]'
            if ($replacement.Length -gt $value.Length) { $replacement = '-' }
            $patches.Add(@{ Offset = $offset + $value.Index * $unitSize; Original = $encoding.GetBytes($value.Value); Replacement = $encoding.GetBytes($replacement.PadRight($value.Length)) })
        }
    }
}
$previousEnd = 0
foreach ($patch in ($patches | Sort-Object { $_.Offset })) {
    if ($patch.Offset -lt $previousEnd -or $patch.Original.Length -ne $patch.Replacement.Length) { throw 'Overlapping or resized diagnostic patch' }
    for ($index = 0; $index -lt $patch.Original.Length; $index++) {
        if ($bytes[$patch.Offset + $index] -ne $patch.Original[$index]) { throw 'Diagnostic data changed during sanitization' }
    }
    [Array]::Copy($patch.Replacement, 0, $bytes, $patch.Offset, $patch.Replacement.Length)
    $previousEnd = $patch.Offset + $patch.Replacement.Length
}
if ($patches.Count) { [IO.File]::WriteAllBytes($binaryPath, $bytes) }
Write-Output ('Removed local paths from ' + $patches.Count + ' diagnostic strings; executable sections and file size preserved')
