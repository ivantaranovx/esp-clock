#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME = esp-clock
IDF_PATH = ~/sources/ESP8266_RTOS_SDK

include $(IDF_PATH)/make/project.mk

FLASH_TOOL = $(ESPTOOLPY) --chip esp8266 --port /dev/ttyUSB0 --baud 2000000

INDEX_ADDR = 0x300000
UP_MAIN_ADDR = 0x110000
UP_INDEX_ADDR = 0x310000

.PHONY:	storage erase upgrade

MINIFIER = ~/.netbeans/minifierbeans/custom-packages/html-minifier-terser
INDEX_SRC = $(PROJECT_PATH)/main/index.html
INDEX_TMP = $(PROJECT_PATH)/tmp/index.html.min
INDEX_GZ = $(PROJECT_PATH)/tmp/index.gz
INDEX_BIN = $(PROJECT_PATH)/tmp/index.bin
FIRMWARE_BIN = $(PROJECT_PATH)/build/esp-clock.bin
UPGRADE_BIN = $(PROJECT_PATH)/tmp/upgrade.eclk

define num2bin
	nc/nc $(1) >>$(2)
endef

define upgrade_header
	echo -n "eclk" >>$(3)
	$(call num2bin,`echo $(1)`,$(3))
	$(call num2bin,0x`crc32 $(2)`,$(3))
endef

define file_header
	$(call num2bin,`stat -c "%s" $(1)`,$(2))
	cat $(1) >>$(2)
endef

storage:
#	$(MINIFIER) "$(INDEX_SRC)" "--minify-js" "true" "--minify-css" "true" "--minify-urls" "true" "--collapse-whitespace" "--keep-closing-slash" "--output" "$(INDEX_TMP)"
	gzip --best --force -n -c $(INDEX_SRC) >$(INDEX_GZ)
	rm -f $(INDEX_BIN)
	$(call file_header,$(INDEX_GZ),$(INDEX_BIN))
	$(FLASH_TOOL) write_flash $(INDEX_ADDR) $(INDEX_BIN)

erase:
	$(FLASH_TOOL) erase_flash

upgrade:
	rm -f $(UPGRADE_BIN)
	$(call upgrade_header,$(UP_MAIN_ADDR),$(FIRMWARE_BIN),$(UPGRADE_BIN))
	$(call file_header,$(FIRMWARE_BIN),$(UPGRADE_BIN))
	$(call upgrade_header,$(UP_INDEX_ADDR),$(INDEX_BIN),$(UPGRADE_BIN))
	$(call file_header,$(INDEX_BIN),$(UPGRADE_BIN))
