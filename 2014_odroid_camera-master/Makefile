LIBS= -lopencv_highgui -lopencv_core -lpthread
CFLAGS= -Wall -g
CPPFLAGS= -Wall -g

all: capture4

capture4: capture4_main.o cam_thread.o usb_camera.o frame_queue.o
	$(CXX) $(CFLAGS) -o capture4 capture4_main.o cam_thread.o usb_camera.o frame_queue.o $(LIBS)


clean:
	rm -f *.o capture4 log.txt
