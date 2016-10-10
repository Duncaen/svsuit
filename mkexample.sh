#!/bin/sh

rm -rf example/
mkdir -p example/run example/service example/sv

# once
for svdir in basic local-fs swap misc; do
	mkdir -p "example/sv/$svdir"
	ln -s "/run/sv/$svdir.supervise" "example/sv/$svdir/supervise"
	touch "example/sv/$svdir/once"
	chmod +x "example/sv/$svdir/once"
done

# basic needs
for svdir in local-fs swap misc; do
	mkdir -p "example/sv/basic/needs"
	ln -s "/service/$svdir" "example/sv/basic/needs/$svdir"
done

# after local-fs
for svdir in misc; do
	mkdir -p "example/sv/$svdir/after"
	ln -s "/service/$svdir" "example/sv/$svdir/after/local-fs"
done

tree example/
