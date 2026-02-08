function exec([string] $cmd) {
	echo $cmd
	iex  $cmd
}

[byte] $mode = 0
if ($args[0] -eq "--mode") {
	if ($args.count -eq 1) {
		write-host "ERROR: --mode given without a value"
		exit 1
	}

	if (-not [byte]::tryparse($args[1], [ref] $mode)) {
		$mode = 0xff # set to an invalid value
	}
}
else {
	$mode = 0
}

if ($mode -gt 3) {
	write-host "ERROR: invalid mode. must be one of 0, 1, 2, 3"
	exit 1
}

if ($mode -eq 0) {
	write-host "usage: build-all [--mode MODE]"
	write-host "mode bit 0: build bit"
	write-host "mode bit 1: benchmark bit"
	write-host "mode 0 is help, and mode 3 both builds and benchmarks"
	exit 0
}

if ($mode -band 1) {
	exec 'make distclean'
	$make = 'make -B CLIP=true SHELL32=false NEIGHBORHOOD=MOORE RULESET=B3/S23 BENCH=false'

	foreach ($profiling in @("false", "true")) {
		foreach ($isa in @("popcnt", "adx", "avx2", "avx512", "native")) {
			if ($profiling -eq "true" -and $isa -ne "popcnt") {
				exec 'sleep -s 3'
			}

			exec "$make ISA=$isa PROFILE=$profiling"
			exec 'ls *.7z | rename-item -newName { $_.name -replace "\.7z$", ".z7" }'
			exec 'make distclean'
		}
	}

	exec 'ls *.z7 | rename-item -newName { $_.name -replace "\.z7$", ".7z" }'
}

if ($mode -band 2) {
	if ($mode -band 1) {
		exec 'sleep -s 5'
	}

	if (!(gcm -type app 7z -ea ignore)) {
		throw 'required program `7z` could not be found.'
	}

	$averages = @{}

	foreach ($zipfile in (ls *.7z | get-random -shuffle)) {
		$name = $zipfile.name.split("-")[2]
		$name = $name.substring(0, $name.length - ".7z".length)

		write-host "benchmarking build: $name"
		7z e -bso0 -bsp0 -y $zipfile life.exe

		$results = @()
		for ($i = 0; $i -lt 5; $i++) {
			$results += @((time -f %e ./life -H nrun 10000000 > $null) 2>&1)
			sleep -s 1
		}

		# convert to string[] first because originally it is ErrorRecord[].
		$results = [double[]] [string[]] $results
		$ave = ($results | measure -ave).average

		write-host "    trials: $($results -join ", ")"
		write-host "    average: $ave"

		$averages.$name = $ave

		sleep -s 3
	}

	write-host "average execution times:"
	$averages.getEnumerator() | sort value
}
