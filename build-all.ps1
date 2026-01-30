function exec([string] $cmd) {
	echo $cmd
	iex  $cmd
}

exec 'make distclean'

$make = 'make -B DEBUG=true CLIP=true SHELL32=false NEIGHBORHOOD=MOORE RULESET=B3/S23'

foreach ($isa in $("popcnt", "avx2", "avx512")) {
	exec "$make ISA=$isa"
	exec 'ls *.7z | rename-item -newName { $_.name -replace "\.7z$", ".z7" }'
	exec 'make distclean'
}

exec "$make ISA=native"
exec 'make clean'
exec 'ls *.z7 | rename-item -newName { $_.name -replace "\.z7$", ".7z" }'
