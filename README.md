# SOMR project.
This project is for SOM++ running on OMR. The target of this project is to enable SOM++ works on OMR, leveraging the GC, JIT , RAS etc from OMR.



# To make it work on your mac:
	git clone https://github.com/zheddie/somr
	git clone https://github.com/eclipse/omr
	cd omr
	make -f run_configure.mk SPEC=osx OMRGLUE=../somr/src/somrvm
	make
	cd ../somr
	make

# To make it work on your linux(this works on REDHL):
	git clone https://github.com/zheddie/somr
	git clone https://github.com/eclipse/omr
	cd omr
	make -f run_configure.mk SPEC=linux_x86-64 OMRGLUE=../somr/src/somrvm
	make
	cd ../somr
	make


# References

SOM++ code base is coming from http://www.hpi.uni-potsdam.de/hirschfeld/projects/som/

OMR project: https://github.com/eclipse/omr