all:
	cd caper; make
	cd capella; make

publish:
	rm -rf /tmp/caper /tmp/caper-*
	svn export . /tmp/caper
	rm -f ~/caper-`date +%Y-%m-%d`.zip
	cd /tmp; zip -r9 /home/naoyuki/caper/caper/site/caper-`date +%Y-%m-%d`.zip caper

