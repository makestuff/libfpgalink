all: classes

classes: jna-3.5.1.jar
	mkdir -p classes
	javac -classpath classes:jna-3.5.1.jar -d classes $$(find src -name "*.java")

test: classes
	java -classpath classes:jna-3.5.1.jar foo.Main

doc: jna-3.5.1.jar
	javadoc -d doc -classpath jna-3.5.1.jar $$(find src/eu/ -name "*.java")

clean: FORCE
	rm -rf classes bin

jna-3.5.1.jar:
	wget -q https://maven.java.net/content/repositories/releases/net/java/dev/jna/jna/3.5.1/jna-3.5.1.jar

FORCE:
