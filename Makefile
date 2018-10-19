build:
	g++ Decomp.cpp -std=c++11 -c -Wall
	g++ Conf.cpp -std=c++11 -c -Wall 
	g++ KeepAlive.cpp -std=c++11 -c -Wall -lmodbus -I /usr/include/mysql/ -L /usr/lib64/mysql/ -I/usr/local/include/modbus -L/usr/local/lib  -lmysqlclient  -lpthread  -lzmq
#	g++ KeepAlive.cpp -std=c++11 -lmodbus -lmysqlclient  -I /usr/include/mysql  -lpthread  -lzmq  -c  -Wall
	g++ Main.cpp -std=c++11  -lpthread  -c -Wall
	g++ Song.cpp  -lzmq -std=c++11 -lpthread -c  -Wall
	g++ g2u.cpp  -std=c++11  -c -Wall
	g++ RecvSnmp.cpp  g2u.cpp  -DHAVE_CONFIG_H  -lsnmp++ -std=c++11   -lpthread  -Wl,-rpath -Wl,/usr/local/lib  -I/usr/local/include  -c -Wall
#	g++ RecvSnmp.cpp  g2u.cpp  -lsnmp++ -std=c++11   -lpthread  -L /usr/local/lib  -I/usr/local/include  -c -Wall
	g++ Song.o  Conf.o  Main.o RecvSnmp.o  Decomp.o  KeepAlive.o  g2u.o  -Wall -lsnmp++ -lmodbus -lpthread -lzmq -levent -g  -std=c++11  -L  /usr/lib64/mysql/ -lmysqlclient -o bin/Demo  -L /usr/local/lib
#	g++ Song.o  Conf.o  Main.o RecvSnmp.o  Decomp.o  KeepAlive.o  Store.o test.o   -lsnmp++ -lmodbus -lpthread -lzmq -levent -g  -std=c++11  -lmysqlclient -Wall -o bin/Demo  
#	g++ Client.cpp -lpthread -std=c++11 -o bin/Client -Wall
clean:
	rm -f *.o
	rm -f bin/Demo
	rm -f bin/Client

install:
	cp -rf  bin/Demo  /usr/local/warning/bin
	cp -rf  bin/Client  /usr/local/warning/bin
	cp -rf  etc/database.conf  /usr/local/warning/etc/
	cp -rf  etc/test.conf   /usr/local/warning/etc/
	cp -rf  lib/libsnmp++.so.33    /usr/local/lib/libsnmp++.so.33
	cp -rf  lib/libzmq.so.5    /usr/local/lib/libzmq.so.5
	ldconfig	

uninstall:
	rm  /usr/local/warning/bin/Demo
	rm  /usr/local/warning/bin/Client
	rm  /usr/local/lib/libsnmp++.so.33
	rm  /usr/local/lib/libzmq.so.5


package:
	tar -czvf  warn.tar.gz  etc/  bin/  lib/ Makefile1
