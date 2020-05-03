PREFIX = ~/.local
CONFIG_PATH = ~/.config/dwmbar

.PHONY: install uninstall

install:
	mkdir -p ${CONFIG_PATH}
	cp ./setup ${CONFIG_PATH}
	cp ./config.sh ${CONFIG_PATH}
	cp -r ./blocks ${CONFIG_PATH}
	mkdir -p ${PREFIX}
	cp {dwmbar,dwmlistener} ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/{dwmbar,dwmlistener}

uninstall:
	rm -rf ${CONFIG_PATH}
	rm ${PREFIX}/bin/{dwmbar,dwmlistener}
