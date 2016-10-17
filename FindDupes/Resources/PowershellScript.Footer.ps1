	"")
$files = $files | ? { $_ -and (Test-Path -LiteralPath $_ ) }

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

	$folder = gi (Split-Path $File)

	while ($folder)
	{
		if ($folder.GetDirectories().Count -ne 0)
		{
			break
		}

		$files_exist = $false
		$exclusions = 'desktop.ini','thumbs.db','md5cache.bin','folder.bin','folder.jpg'
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



