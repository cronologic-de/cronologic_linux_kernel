# Please make the file under the sub-directory related to your target architecture

SUBDIRS := $(wildcard */.) 

SUBDIRS: 
	for dir in $(SUBDIRS) ; do \
		make -C $$dir; \
	done

clean:
	for dir in $(SUBDIRS) ; do \
		make -C $$dir clean; \
	done
