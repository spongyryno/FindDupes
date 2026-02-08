	"")

if (-not $DontCheckExistence)
{
	$numFiles = $files.Count - 1

	Write-Host ("Checking {0:#,0} files for existence..." -f $numFiles)
	$t0 = Get-Date
	$files = $files | ? { $_ -and (Test-Path -LiteralPath $_ ) }
	$t1 = Get-Date
	Write-Host ("...it took {0:#,0.000} seconds." -f ($t1-$t0).TotalSeconds)

	if ($files.Count -eq $numFiles)
	{
		Write-Host ("All {0:#,0} files exist." -f $numFiles)
	}`
	else
	{
		Write-Host ("Pared down file count from {0:#,0} to {1:#,0} (removed {2:#,0} files that don't exist)." -f $numFiles, $files.Count, ($numFiles-$files.Count))
	}
}

function _Delete
{
	param([string]$File, [switch]$DoIt)

	if ($DoIt)
	{
		Remove-Item -Force -ErrorAction SilentlyContinue -LiteralPath $File
		Write-Host -Fore Gray $File
	}
	else
	{
		Write-Host -Fore Red $File
	}

	$folder = Get-Item -Force -Path (Split-Path -Parent -Path $File)

	while ($folder)
	{
		if ($folder.GetDirectories().Count -ne 0)
		{
			break
		}

		$files_exist = $false
		$exclusions = 'desktop.ini','thumbs.db','md5cache.bin','md5cache.md5','folder.bin','folder.jpg'
		$folder.GetFiles() | ? { $exclusions -notcontains $_.Name } | % { $files_exist = $true }

		if ($files_exist)
		{
			break
		}

		# delete all exclusions
		$folder.GetFiles() | ? { $exclusions -contains $_.Name } | % { $_.IsReadOnly = $false;$_.Delete() }

		# delete the folder
		$folder.Delete()

		Write-Host -Fore White $folder.FullName
		$folder = $folder.Parent
	}
}

$files | % -begin {$i=0} -process { Write-Progress -Activity "Deleting ""$_""..." -PercentComplete (100.0 * $i / $files.Length);_Delete -DoIt:$DoIt $_;++$i }

if (-not $DoIt)
{
	Write-Host -Fore White "`nNot actually doing anything since the ""-DoIt"" option was not specified.`n"
}


