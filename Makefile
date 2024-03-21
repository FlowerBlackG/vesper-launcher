# vesper launcher 辅助构建脚本
#
# 创建于 2024年2月25日 京沪高铁上

.DEFAULT_GOAL := all

.PHONY: help
help:
	@echo "vesper launcher makefile"
	@echo "---------------"
	@echo "commands available:"
	@echo "- make run"
	@echo "    build and launch vesper launcher"
	@echo "- make build"
	@echo "    build vesper launcher and copy the binary to \"target\" folder"
	@echo "- make"
	@echo "    alias for \"make all\""

.PHONY: prepare
prepare:
	mkdir -p build && cd build \
	&& cmake -G"Unix Makefiles" ../src


.PHONY: build
build: prepare
	cd build && cmake --build . -- -j 8
	mkdir -p target && cp build/vesper-launcher target/
	cd target && mkdir -p asm-dump \
	&& objdump -d ./vesper-launcher > asm-dump/vesper-launcher.text.asm \
	&& objdump -D ./vesper-launcher > asm-dump/vesper-launcher.full.asm 
	@echo -e "\033[32mbuild success (vesper launcher).\033[0m"


.PHONY: clean
clean:
	rm -rf ./build
	rm -rf ./target


.PHONY: run
run: build
	cd target && ./vesper-launcher \
		--domain-socket vesper-launcher.sock \
		--daemonize \
		--wait-for-child-before-exit


.PHONY: run-no-args
run-no-args: build
	cd target && ./vesper-launcher


.PHONY: install
install: build
	cp target/vesper-launcher /usr/sbin/vesper-launcher


.PHONY: uninstall
uninstall:
	rm -f /usr/sbin/vesper-launcher


.PHONY: all
all: build
