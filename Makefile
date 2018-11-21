include $(GASNET_PREFIX)/include/udp-conduit/udp-par.mak

CC         := $(GASNET_CC)
CXX        := $(GASNET_CXX)
LD         := $(GASNET_LD)

TARGET     := libhiccup.a
SRCDIR     := src
INCDIR     := include
BUILDDIR   := obj
SRCEXT     := c
DEPEXT     := d
OBJEXT     := o

CFLAGS     := $(GASNET_CFLAGS) -fdiagnostics-color=always
LDFLAGS    := $(GASNET_LDFLAGS) -L$(GASNET_PREFIX)
LIB        := $(GASNET_LIBS) -lgasnet_tools-par
INC        := $(GASNET_CPPFLAGS) -I$(INCDIR) -DGASNETT_THREAD_SAFE=1
INCDEP     := -I$(INCDIR)

$(info $(value CC))
$(info $(value CXX))
$(info $(value LD))
$(info $(value CFLAGS))
$(info $(value LDFLAGS))
$(info $(value LIB))
$(info $(value INC))
$(info $(value INCDEP))

#---------------------------------------------------------------------------------
#DO NOT EDIT BELOW THIS LINE
#---------------------------------------------------------------------------------

SOURCES    := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS    := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

#Defauilt Make
all: resources $(TARGET)

#Remake
remake: cleaner all

#Copy Resources from Resources Directory to Target Directory
resources: directories

#Make the Directories
directories:
	@mkdir -p $(BUILDDIR)

#Clean objects and binary / library
clean:
	@$(RM) -rf $(BUILDDIR)
	@$(RM) -f $(TARGET)

#Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

#If building an executable instead of shared library, uncomment the next couple of lines to link libraries
#$(TARGET): $(OBJECTS)
#   $(LD) $(LDFLAGS) -o $(TARGET) $^ $(LIB)

#Else create static library
$(TARGET): $(OBJECTS)
	ar rcs $(TARGET) $(OBJECTS)

#Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<
	@$(CC) $(CFLAGS) $(INCDEP) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp
