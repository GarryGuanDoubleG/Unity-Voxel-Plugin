SRCDIR = ../../source
SRCS = $(SRCDIR)/RenderingPlugin.cpp \
$(SRCDIR)/RenderAPI.cpp \
$(SRCDIR)/RenderAPI_OpenGLCoreES.cpp
OBJS = ${SRCS:.cpp=.o}
UNITY_DEFINES = -DSUPPORT_OPENGL_UNIFIED=1 -DUNITY_LINUX=1
GLEW_CFLAGS = $(shell pkg-config --cflags glew)
GLEW_LIBS = $(shell pkg-config --libs glew)
CXXFLAGS = $(UNITY_DEFINES) -O2 -fPIC $(GLEW_CFLAGS)
LDFLAGS = -shared -rdynamic
LIBS = $(GLEW_LIBS)
PLUGIN_SHARED = libRenderingPlugin.so
CXX ?= g++

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: shared

clean:
	rm -f $(OBJS) $(PLUGIN_SHARED)

shared: $(OBJS)
	$(CXX) $(LDFLAGS) -o $(PLUGIN_SHARED) $(OBJS) $(LIBS)
