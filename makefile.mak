#set default parameters
platform := $(if $(platform),$(platform),linux)
mode := $(if $(mode),$(mode),debug)

ifneq ($(or $(pic),$(dynamic_name)),)
pic := 1
suffix := _pic
endif

#set directories
include_dirs := $(path_step)/include $(path_step)/src $(path_step) $(include_path)
libs_dir := $(path_step)/lib/$(mode)$(suffix)
objs_dir := $(path_step)/obj/$(mode)$(suffix)
bins_dir := $(path_step)/bin/$(mode)

#set platform-specific parameters
include $(path_step)/make/platforms/$(platform).mak

#set features
include $(path_step)/features.mak

#tune output according to type
ifdef library_name
output_dir := $(libs_dir)
objects_dir := $(objs_dir)/$(library_name)
target := $(output_dir)/$(call makelib_name,$(library_name))
else ifdef binary_name
output_dir := $(bins_dir)
objects_dir := $(objs_dir)/$(binary_name)
target := $(output_dir)/$(call makebin_name,$(binary_name))
else ifdef dynamic_name
output_dir := $(bins_dir)
objects_dir := $(objs_dir)/$(dynamic_name)
target := $(output_dir)/$(call makedyn_name,$(dynamic_name))
pic := 1
else
$(error Invalid target)
endif

#main target
all: dirs $(target)
.PHONY: all dirs

#setup environment
definitions += __STDC_CONSTANT_MACROS

#set compiler-specific parameters
include $(path_step)/make/compilers/$(compiler).mak

#calculate input source files
ifdef source_dirs
source_files := $(wildcard $(addsuffix /*.cpp,$(source_dirs)))
else ifdef source_files
source_files := $(source_files:=.cpp)
else
$(error Not source_dirs or source_files defined at all)
endif

#calculate object files from sources
object_files := $(notdir $(source_files))
object_files := $(addprefix $(objects_dir)/,$(object_files:.cpp=$(call makeobj_name,)))

#make objects and binaries dir
dirs:
	@echo Building $(if $(library_name),library $(library_name),\
	  $(if $(binary_name),executable $(binary_name),dynamic object $(dynamic_name)))
	mkdir -p $(objects_dir)
	mkdir -p $(output_dir)

#build target
ifdef library_name
#simple libraries
$(target): $(object_files)
	$(build_lib_cmd)
else
#binary and dynamic libraries with dependencies
.PHONY: deps $(depends)

deps: $(depends)

$(depends):
	$(MAKE) -C $(addprefix $(path_step)/,$@) $(if $(pic),pic=1,) $(MAKECMDGOALS)

$(target): deps $(object_files) $(foreach lib,$(libraries),$(libs_dir)/$(call makelib_name,$(lib)))
	$(link_cmd)
	$(postlink_cmd)
endif

VPATH := $(source_dirs)

$(objects_dir)/%$(call makeobj_name,): %.cpp
	$(build_obj_cmd)

.PHONY: clean clean_all

clean:
	rm -f $(target)
	rm -Rf $(objects_dir)

clean_all: clean clean_deps

clean_deps: $(depends)

#show some help
help:
	@echo "Targets:"
	@echo " all - build target (default)"
	@echo " clean - clean all"
	@echo " help - this page"
	@echo "Accepted flags via flag=value options for make:"
	@echo " mode - compilation mode (release,debug). Default is 'release'"
	@echo " profile - enable profiling. Default no"
	@echo " arch - selected architecture. Default is 'native'"
	@echo " platform - selected platform. Default is 'linux'"
	@echo " CXX - used compiler. Default is 'g++'"
	@echo " cxx_flags - some specific compilation flags"
	@echo " LD - used linker. Default is 'g++'"
	@echo " ld_flags - some specific linking flags"
