BUILD_SUBDIRS = vm filesys

all::
	@echo "Run 'make' in subdirectories: $(BUILD_SUBDIRS)."
	@echo "This top-level make has only 'clean' targets."

CLEAN_SUBDIRS = $(BUILD_SUBDIRS) examples utils

clean::
	for d in $(CLEAN_SUBDIRS); do $(MAKE) -C $$d $@; done
	rm -f TAGS tags

distclean:: clean
	find . -name '*~' -exec rm '{}' \;

TAGS_SUBDIRS = $(BUILD_SUBDIRS) devices lib
TAGS_SOURCES = find $(TAGS_SUBDIRS) -name \*.[chS] -print

TAGS::
	etags --members `$(TAGS_SOURCES)`

tags::
	ctags -T --no-warn `$(TAGS_SOURCES)`

cscope.files::
	$(TAGS_SOURCES) > cscope.files

cscope:: cscope.files
	cscope -b -q -k

##################
# Handin your work
##################
turnin.tar: clean
	tar cf turnin.tar `find . -type f | grep -v '^\.*$$' | grep -v '/CVS/' | grep -v '/\.svn/' | grep -v '/\.git/' | grep -v '*\.tar\.gz' | grep -v '/\~/' | grep -v '/\.txt' | grep -v '/\.pl' |grep -v '/\.tar/'` 

PART1_NAME := vm
PART2_NAME := fs

turnin_vm: turnin.tar
	echo "Preparing vm_turnin.tar containing the following files:"
	tar tf turnin.tar
	mv turnin.tar vm_turnin.tar
	echo "Please submit vm_turnin.tar through Canvas"

turnin_fs: turnin.tar
	echo "Preparing fs_turnin.tar containing the following files:"
	tar tf turnin.tar
	mv turnin.tar fs_turnin.tar
	echo "Please submit fs_turnin.tar through Canvas"
