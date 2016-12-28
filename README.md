# SOMR project.
This project is for SOM++ running on OMR. The target of this project is to enable SOM++ works on OMR, leveraging the GC, JIT , RAS etc from OMR.

Base code is coming from http://www.hpi.uni-potsdam.de/hirschfeld/projects/som/

# To make it work on your mac:
	git clone https://github.com/zheddie/somr
	git clone https://github.com/eclipse/omr
	cd omr
	make -f run_configure.mk SPEC=mac OMRGLUE=../somr/src/somrvm
	make
	cd ../somr
	make

# To make it work on your linux: TODO.
	

