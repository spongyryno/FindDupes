<#

.SYNOPSIS
A script to find duplicates files under a given parent folder.

.DESCRIPTION
The script recursively looks through a folder for all files, capturing information about them and caching it in an XML, and then
finding and reporting information about duplicate files. It can find duplicated even if they have different names and are in
different tree structures. It uses the file size as the first test, so it rarely ever needs to even look at a file's contents. If
it finds two files with the same length, it computes a hash of each file, which is caches in the XML, and compares the hashes to
determine if the files are duplicate.

Once the script has been run, the information it gathered is persistent in the XML. That includes the file's write time, and the
script is smart enough to use this to determine if it needs to recalculate a file's hash, or if it can just use the cached value.

.PARAMETER <Parameter-Name>

.EXAMPLE

.NOTES

.LINK

#>

param(
	[string]$XmlCacheFile=$null,
	[string]$Path="$($pwd.Path)",
	[UInt32]$Verbosity=1,
	[UInt32]$MinimumSize=1048576,
	[switch]$ContinuousUpdate,
	[switch]$Trim,
	[string]$RestrictOutputPath=$null
)

cls

if ($false)
{
	$Verbosity=6
	$ContinuousUpdate=$false
	$Path="R:\mike\documents\Financial"
	$XmlCacheFile="R:\dupes.xml"
}

###################################################################################################
# get the location of "this"
$script:loc = $null

switch ($myInvocation.MyCommand.CommandType)
{
	([Management.Automation.CommandTypes]::ExternalScript)	{ $script:loc = Split-Path $myInvocation.MyCommand.Definition } # run as command
	#([Management.Automation.CommandTypes]::Script)			{ $script:loc = $pwd.Path } # pasted into the command window
	([Management.Automation.CommandTypes]::Script)			{ $script:loc = "F:\OldMike-PC\users\mlyons\depot\bin" }
}

###################################################################################################
# Provide a way to suppress Ctrl-C temporarily while we are doing something critical that we
# don't want interrupted, even though we do want the user to be able to stop this script from
# executing with Ctrl-C in general.
###################################################################################################
Add-Type -Path "$loc\ctrlc.cs"

###################################################################################################
###################################################################################################
function Get-ElapsedTimeString
{
	param([DateTime]$StartTime, [DateTime]$EndTime=(Get-Date))

	[UInt64]$ticks_per_millisecond = 10000
	[Uint64]$milliseconds_per_second = 1000
	[UInt64]$seconds_per_minute = 60
	[UInt64]$minutes_per_hour = 60
	[UInt64]$hours_per_day = 24

	$ticks = ($EndTime - $StartTime).Ticks
	$elapsed_time = ($EndTime - $StartTime).TotalSeconds

	[long]$sec=0
	[long]$min=0
	[long]$hr=0

	$ticks = [Math]::DivRem($ticks, $ticks_per_millisecond * $milliseconds_per_second * $seconds_per_minute, [ref]$sec)
	$ticks = [Math]::DivRem($ticks, $minutes_per_hour, [ref]$min)
	$ticks = [Math]::DivRem($ticks, $hours_per_day, [ref]$hr)
	$day = $ticks

	[double]$sec = ([double]$sec) / ($ticks_per_millisecond * $milliseconds_per_second)

	if ($day -gt 0)
	{
		"{0} days, {1:F0}:{2:00}:{3:0#}." -f $day, $hr, $min, $sec
	}
	elseif ($hr -gt 0)
	{
		"{0:F0}:{1:00}:{2:0#}." -f $hr, $min, $sec
	}
	elseif ($min -gt 0)
	{
		"{0:00}:{1:0#.####}." -f $min, $sec
	}
	else
	{
		"{0} seconds." -f $sec
	}

}

###################################################################################################
###################################################################################################
function Write-ElapsedTime
{
	param([DateTime]$StartTime, [DateTime]$EndTime=(Get-Date))

	Write-Host -ForegroundColor Yellow "Command took $(Get-ElapsedTimeString $StartTime $EndTime)"
}

###################################################################################################
###################################################################################################
Add-Type -TypeDefinition @"
    public class FileOnDisk
    {
        public string               Name;
        public string               Path;
        public ulong                Size;
        public System.DateTime      Time;
		public bool					Hashed;
        public byte[]               Hash;
    }
"@

###################################################################################################
###################################################################################################
function Write-Verbose
{
	param(
		[UInt32]$Level,
		[string]$output,
		[ConsoleColor]$ForeGroundColor=[ConsoleColor]::Gray,
		[ConsoleColor]$BackGroundColor=[ConsoleColor]::Black,
		[switch]$NoNewLine)

	if ($Verbosity -ge $level)
	{
		Write-Host -ForegroundColor $ForeGroundColor -BackgroundColor $BackgroundColor -NoNewLine:$NoNewLine $output
	}
}

###################################################################################################
###################################################################################################
function Get-FileHash
{
	param([string]$fileName, [Security.Cryptography.HashAlgorithm]$hashAlgorithm=$null)

	if (-not $hashAlgorithm)
	{
		$hashAlgorithm = [Security.Cryptography.HashAlgorithm]::Create("MD5")
	}

	try
	{
		$stream = New-Object System.IO.FileStream($fileName, [IO.FileMode]::Open, [IO.FileAccess]::Read)
		$hashValue = $hashAlgorithm.ComputeHash($stream)
		$stream.Dispose()
		$stream = $null

		return $hashValue
	}
	catch
	{
	}
}

###################################################################################################
###################################################################################################
function Ensure-FileIsHashed
{
	param([FileOnDisk]$fileOnDisk, [Security.Cryptography.HashAlgorithm]$hashAlgorithm=$null)

	if ( -not $fileOnDisk.Hashed )
	{
		#TEMP Write-Verbose 3 -Fore Yellow "Calculating MD5 hash of $($fileOnDisk.Path)..."
		Write-Verbose 1 -Fore Yellow "Calculating MD5 hash of $($fileOnDisk.Path)..."
		$fileOnDisk.Hash = Get-FileHash $fileOnDisk.Path $hashAlgorithm
		$fileOnDisk.Hashed = $true

		return $true
	}
	else
	{
		Write-Verbose 4 -ForegroundColor DarkGreen "Hash already current for $($fileOnDisk.Path)..."
	}

	return $false
}


###################################################################################################
###################################################################################################
$LastTimeOutputCacheWasRun = $null
function Output-Cache
{
	param([switch]$OnlyIfNecessary, [string]$info="", [double]$SkipTime=30.0)

	if ($XmlCacheFile)
	{
		if ($OnlyIfNecessary)
		{
			if ($LastTimeOutputCacheWasRun)
			{
				$delta = (Get-Date) - $LastTimeOutputCacheWasRun

				if ($delta.TotalMinutes -le $SkipTime)
				{
					$pos = $host.UI.RawUI.CursorPosition
					$string = "Skipping Output-Cache since it was run within the last $SkipTime minutes. $info"
					if ($host.UI.RawUI.BufferSize.Width -gt $string.Length)
					{
						Write-Verbose 2 -ForegroundColor Blue ($string + (" "*($host.UI.RawUI.BufferSize.Width - $string.Length - 1)))
					}
					else
					{
						Write-Verbose 2 -ForegroundColor Blue $string
					}
					$host.UI.RawUI.CursorPosition = $pos
					return
				}
				else
				{
					Write-Verbose 2 -ForegroundColor Blue ("Not skipping Output-Cache since the last run was {0} minutes ago." -f $delta.TotalMinutes)
				}
			}
		}

		$SCRIPT:LastTimeOutputCacheWasRun = Get-Date

		$ocstart = Get-Date
		Write-Verbose 2 -ForegroundColor Yellow "Output-Cache starting..."

		$FilesToOutput = @{}

		if ($FilesInXmlHash)
		{
			$start = Get-Date
			Write-Verbose 2 -ForegroundColor Yellow -NoNewLine "Adding to XML..."
			$FilesOnDiskHash.keys | sort | % { $FilesInXmlHash[$_] = $FilesOnDiskHash[$_] }
			Write-Verbose 2 -ForegroundColor DarkYellow "Command took $(Get-ElapsedTimeString $start)"

			$start = Get-Date
			Write-Verbose 2 -ForegroundColor Yellow -NoNewLine "Sorting keys..."
			$FilesInXmlHash.keys | sort | % { $FilesToOutput[$_] = $FilesInXmlHash[$_] }
			Write-Verbose 2 -ForegroundColor DarkYellow "Command took $(Get-ElapsedTimeString $start)"
		}
		else
		{
			$start = Get-Date
			Write-Verbose 2 -ForegroundColor Yellow -NoNewLine "Sorting keys..."
			$FilesOnDiskHash.keys | sort | % { $FilesToOutput[$_] = $FilesOnDiskHash[$_] }
			Write-Verbose 2 -ForegroundColor DarkYellow "Command took $(Get-ElapsedTimeString $start)"
		}

		if (Test-Path $XmlCacheFile)
		{
			$start = Get-Date
			Write-Verbose 2 -ForegroundColor Yellow -NoNewLine "Backing up old xml cache file..."
			$xmlItem = gi $XmlCacheFile
			Copy ($xmlItem.FullName) ($xmlItem.FullName+".bak")
			Write-Verbose 2 -ForegroundColor DarkYellow "Command took $(Get-ElapsedTimeString $start)"
		}

		try
		{
			$start = Get-Date
			$ctrlc_pressed = $false
			$ctrlc = New-Object SpongySoft.ControlCSuppressor
			$ctrlc.PauseControlCProcessing()
			Write-Verbose 2 -ForegroundColor Yellow -NoNewLine "Exporting xml..."
			Export-Clixml $XmlCacheFile -InputObject $FilesToOutput
			Write-Verbose 2 -ForegroundColor DarkYellow "Command took $(Get-ElapsedTimeString $start)"
		}
		finally
		{
			$ctrlc_pressed = $ctrlc.ResumeControlCProcessing()
			$ctrlc.Dispose()
			$ctrlc = $null
		}

		if ($ctrlc_pressed)
		{
			Write-Host -Fore Red "<Ctrl-C> detected during processing. Exiting."
			exit
		}

		Write-Verbose 2 -ForegroundColor DarkYellow "   Output-Cache took $(Get-ElapsedTimeString $ocstart)"
	}
}

###################################################################################################
###################################################################################################
function findfiles
{
	param([IO.DirectoryInfo]$Path, [string]$ExcludeFile=$null)

	foreach ($file in (gci -Path $Path.FullName -Force | ? { $_.FullName -ne $ExcludeFile }))
	{
		if (($file.PSIsContainer) -and (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0))
		{
			findfiles -Path $file -ExcludeFile $ExcludeFile
		}
		else
		{
			$file
		}
	}
}

###################################################################################################
###################################################################################################
$FilesInXmlHash = $null
$XmlCacheFilePath = $null

if ($XmlCacheFile)
{
	if (Test-Path $XmlCacheFile)
	{
		$XmlCacheFilePath = (gi $XmlCacheFile).FullName
		$start = Get-Date
		Write-Verbose 1 -ForegroundColor Yellow -NoNewline "Reading source data from XML cache file..."
		$FilesInXmlHash = Import-Clixml $XmlCacheFile
		Write-Verbose 1 -ForegroundColor Yellow "Found $($FilesInXmlHash.Count) files in $(Get-ElapsedTimeString $start)..."
	}
}


###################################################################################################
###################################################################################################
if ($Trim)
{
	if ($true)
	{
		$start = Get-Date
		Write-Verbose 1 -ForegroundColor Yellow -NoNewline "Removing files that don't exist..."
		$FilesToOutput = @{}
		$fileCount = [UInt32]0
		$pos = $host.UI.RawUI.CursorPosition
		foreach ($key in ($FilesInXmlHash.keys | sort))
		{
			if (($fileCount % 100) -eq 0)
			{
				$host.UI.RawUI.CursorPosition = $pos
				Write-Verbose 1 -ForegroundColor DarkGray -NoNewline ("({0:#,#} of {1:#,#})" -f $fileCount, $FilesInXmlHash.Count)
			}
			$fileCount++

			if (Test-Path "$key")
			{
				$FilesToOutput[$key] = $FilesInXmlHash[$key]
			}
		}
		Write-Verbose 1 -ForegroundColor Yellow "...command took $(Get-ElapsedTimeString $start)..."
		Write-Verbose 1 -ForegroundColor Gray ("Went from {0:#,#} files to {1:#,#} files." -f $FilesInXmlHash.Count, $FilesToOutput.Count)
	}

	if ($true)
	{
		$start = Get-Date
		Write-Verbose 1 -ForegroundColor Yellow -NoNewline "Trim cleanup..."
		$FilesInXmlHash = $FilesToOutput
		$FilesToOutput = $null
		[GC]::Collect()
		Write-Verbose 1 -ForegroundColor Yellow "...command took $(Get-ElapsedTimeString $start)..."
	}
}


###################################################################################################
###################################################################################################
# recursively get all files
$start = Get-Date
Write-Verbose 1 -ForegroundColor Yellow -NoNewline "Getting local file info from the filesystem..."
$filesOnDisk = findfiles -Path (gi -Force $Path)
Write-Verbose 1 -ForegroundColor Yellow "Found $($filesOnDisk.Length) files in $(Get-ElapsedTimeString $start)..."

###################################################################################################
# create the hashtable
Write-Verbose 1 -ForegroundColor Yellow "Creating the data structure..."
$FilesOnDiskHash = @{}

# process the files
$start = Get-Date
if ($Verbosity) { Write-Verbose 1 -ForegroundColor Yellow -NoNewline "Adding files to the data structure (hashtable)..." }

$pos = $host.UI.RawUI.CursorPosition
$host.UI.RawUI.CursorPosition = $pos
$fileCount = [UInt32]0

###################################################################################################
# Update the FilesOnDiskHash with info about the files, including hash info if it's in the
# database (XML hash)
###################################################################################################
foreach ($fileOnDisk in $filesOnDisk)
{
	if ((($fileCount % 100) -eq 0) -or ($fileCount -eq ($filesOnDisk.Count - 1)))
	{
		$host.UI.RawUI.CursorPosition = $pos
		Write-Verbose 1 -ForegroundColor DarkGray -NoNewline ("({0:#,#} of {1:#,#})" -f $fileCount, $filesOnDisk.Count)
	}
	$fileCount++

	# prepare the object to go into the hashtable
	$fileInfo = New-Object FileOnDisk

	$fileInfo.Name = $fileOnDisk.Name
	$fileInfo.Path = $fileOnDisk.FullName
	$fileInfo.Time = $fileOnDisk.LastWriteTimeUtc
	$fileInfo.Size = $fileOnDisk.Length
	$fileInfo.Hashed = $false;

	# see if this file is in the Xml cached file info
	if ($FilesInXmlHash)
	{
		$fileInXml = $FilesInXmlHash[$fileOnDisk.FullName]
		if ($fileInXml)
		{
			# we found it... but is the file newer than the cached copy?
			if ($fileInXml.Time -ge $fileOnDisk.LastWriteTimeUtc)
			{
				# the file exists, and is in the cache, and the cached copy is as recent as the actual file,
				# so if we have already calculated the hash, let's not do it again

				# this really shouldn't be different... but just in case it is...
				if ($Verbosity -ge 2)
				{
					if ($fileInXml.Time -gt $fileOnDisk.LastWriteTimeUtc)
					{
						Write-Error "The file timestamp in the cache should never be newer than the actual file.`nFile`: $($fileOnDisk.FullName)"
						$pos = $host.UI.RawUI.CursorPosition
					}
				}

				$fileInXml.Time = $fileOnDisk.LastWriteTimeUtc

				# this step shouldn't be necessary, because any time that the hash value is NULL, the
				if (-not $fileInXml.Hash)
				{
					if (($fileInXml.Hashed) -and ($fileInXml.Length -gt 0))
					{
						Write-Error "File $($fileInXml.Path) has no hash, but believes that it should."
						$pos = $host.UI.RawUI.CursorPosition
						$fileInXml.Hashed = $false
					}
				}

				# copy over all the data from the cached info
				$thisVerbosity = 6
				if ($fileInXml.Hashed)
				{
					$thisVerbosity = 5
				}

				Write-Verbose $thisVerbosity -Fore Red  -NoNewLine "File info for "
				Write-Verbose $thisVerbosity -Fore Blue -NoNewLine "$($fileOnDisk.FullName)"
				Write-Verbose $thisVerbosity -Fore Red  -NoNewLine " is up-to-date (Hash`: $($fileInXml.Hashed))`n"

				$fileInfo.Hashed = $fileInXml.Hashed
				$fileInfo.Hash = $fileInXml.Hash
			}
		}
	}

	$FilesOnDiskHash[$fileOnDisk.FullName] = $fileInfo
}
Write-Verbose 1 -ForegroundColor DarkYellow "   Command took $(Get-ElapsedTimeString $start)"

# $FilesOnDiskHash.GetEnumerator() | ? { $_.Value.Hash } | % { $_.Key }

# make it a native array, sorted by size
Write-Verbose 1 -ForegroundColor Yellow "Sorting based on size..."
$SortedFilesOnDisk = $FilesOnDiskHash.keys | % { $FilesOnDiskHash[$_] } | Sort -property "Size"

$start = Get-Date
Write-Verbose 1 -ForegroundColor Yellow "Checking for duplicate sizes in $($SortedFilesOnDisk.Length) files..."
$md5 = [Security.Cryptography.HashAlgorithm]::Create("MD5")

[UInt64]$totalSize = 0
[UInt32]$totalCount = 0

# loop through all the sorted files
for ($i = 0 ; $i -lt $SortedFilesOnDisk.Length-2 ; $i++)
{
	# if two adjacent files have the same size, they MAY be identical
	if ($SortedFilesOnDisk[$i].Size -eq $SortedFilesOnDisk[$i+1].Size)
	{
		Write-Verbose 5 -Fore Blue "Hashed`: $($SortedFilesOnDisk[$i].Hashed) ($($SortedFilesOnDisk[$i].Path))"
		Write-Verbose 5 -Fore Blue "Hashed`: $($SortedFilesOnDisk[$i+1].Hashed) ($($SortedFilesOnDisk[$i+1].Path))"

		$hashCalculated1 = Ensure-FileIsHashed ([ref]($SortedFilesOnDisk[$i])) $md5
		$hashCalculated2 = Ensure-FileIsHashed ([ref]($SortedFilesOnDisk[$i+1])) $md5

		if ($hashCalculated1 -or $hashCalculated2)
		{
			if ($ContinuousUpdate)
			{
				# if either hash was calculated, and we have a continuous update, then update the output cache
				# here so that if we ctrl-break (or ctrl-C), we will have captured the output
				Output-Cache -OnlyIfNecessary ($SortedFilesOnDisk[$i].Name)
			}
		}

		Write-Verbose 5 -Fore Blue "Hashed`: $($SortedFilesOnDisk[$i].Hashed) ($($SortedFilesOnDisk[$i].Path))"
		Write-Verbose 5 -Fore Blue "Hashed`: $($SortedFilesOnDisk[$i+1].Hashed) ($($SortedFilesOnDisk[$i+1].Path))"

		# ignore cases where we have no hash because the file couldn't be opened
		if ($SortedFilesOnDisk[$i].Hash -and $SortedFilesOnDisk[$i+1].Hash)
		{
			for ($j = 0 ; $j -lt $SortedFilesOnDisk[$i].Hash.Length ; $j++)
			{
				if ($SortedFilesOnDisk[$i].Hash[$j] -ne $SortedFilesOnDisk[$i+1].Hash[$j])
				{
					break
				}
			}

			if ($j -eq $SortedFilesOnDisk[$i].Hash.Length)
			{
				if ($SortedFilesOnDisk[$i].Size -ge $MinimumSize)
				{
					if ($false)
					{
						Write-Output """$($SortedFilesOnDisk[$i].Path)"" and ""$($SortedFilesOnDisk[$i+1].Path)"" are the same."
					}
					else
					{
						$print=$false;
						$onlyOne=$false;

						# check the $RestrictOutputPath variable...
						if ($SortedFilesOnDisk[$i].Path.StartsWith($RestrictOutputPath, [StringComparison]::InvariantCultureIgnoreCase))
						{
							$print = $true
							$onlyOne = -not $onlyOne
							$item1 = $SortedFilesOnDisk[$i]
							$item2 = $SortedFilesOnDisk[$i+1]
						}

						if ($SortedFilesOnDisk[$i+1].Path.StartsWith($RestrictOutputPath, [StringComparison]::InvariantCultureIgnoreCase))
						{
							$print = $true
							$onlyOne = -not $onlyOne
							$item1 = $SortedFilesOnDisk[$i+1]
							$item2 = $SortedFilesOnDisk[$i]
						}

						if ($print)
						{
							$totalSize += $SortedFilesOnDisk[$i].Size
							$totalCount ++
							if ($onlyOne)
							{
								$oldColor = [Console]::ForegroundColor
								[Console]::ForegroundColor = [ConsoleColor]::Yellow
							}

							Write-Output ("{0,18:#,#} {1}" -f $item1.Size, $item1.Path)

							if ($onlyOne)
							{
								[Console]::ForegroundColor = $oldColor
							}

							Write-Output ("{0,18:#,#} {1}" -f $item2.Size, $item2.Path)
							Write-Output ""
						}
					}
				}
			}
		}
	}
}

Write-Verbose 1 -ForegroundColor Gray  ("Number of files: {0:#,#}" -f $totalCount)
Write-Verbose 1 -ForegroundColor Gray  ("Total size:      {0:#,#}" -f $totalSize)

Write-Verbose 1 -ForegroundColor DarkYellow "Command took $(Get-ElapsedTimeString $start)"

Output-Cache

return


