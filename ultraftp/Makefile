JDEST = ../../../../bin/util/ultraftp.jar

SOURCES = $(shell echo *.java)
CLASSES = $(SOURCES:%.java=%.class)
ARCHIVE = ultraftp.jar
JC_ARGS = -target 1.4 -source 1.4

all: $(CLASSES) $(ARCHIVE)

$(CLASSES): $(SOURCES)
	javac $(JC_ARGS) $(SOURCES)

$(ARCHIVE): $(CLASSES)
	jar cvf $(ARCHIVE) *.class
	ls -l $(ARCHIVE)

clean:
	rm -f *.class *.jar

install: $(ARCHIVE)
	@if [ -f $(JDEST) ]; then mv $(JDEST) $(JDEST).prev; fi
	cp $(ARCHIVE) $(JDEST)
	ls -l $(JDEST)*

