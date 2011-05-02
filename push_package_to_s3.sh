#!/bin/sh
# François-Xavier Bourlet <fx@dotcloud.com>
# purpose: upload debian packages to s3://dotcloud-deb

PATTERN="../lxc-dotcloud_*-*_*.deb ../lxc-dotcloud_*-*.dsc"

. /etc/lsb-release
for pkg in `ls $PATTERN`
do
	echo "uploading $pkg ..."
	s3cmd put "$pkg" s3://dotcloud-deb/ubuntu-$DISTRIB_RELEASE/
	s3cmd setacl -P -r s3://dotcloud-deb
done
