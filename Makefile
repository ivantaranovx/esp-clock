#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp-clock

include $(IDF_PATH)/make/project.mk

FLASH_TOOL := $(ESPTOOLPY) --chip esp8266 --port /dev/ttyUSB0 --baud 2000000
PART_ADDR := 0x300000

.PHONY:	storage erase read read__nvs

storage:
	/home/alex/.netbeans/minifierbeans/custom-packages/html-minifier-terser "$(PROJECT_PATH)/main/index.html" "--minify-js" "true" "--minify-css" "true" "--minify-urls" "true" "--collapse-whitespace" "--keep-closing-slash" "--output" "$(PROJECT_PATH)/tmp/index.html.min"
	gzip --best --force -n -c $(PROJECT_PATH)/tmp/index.html.min >$(PROJECT_PATH)/tmp/index.gz
	stat -c "%s" $(PROJECT_PATH)/tmp/index.gz | xargs printf "0: %.8x" | xxd -r -g4 >$(PROJECT_PATH)/tmp/index.bin
	cat $(PROJECT_PATH)/tmp/index.gz >>$(PROJECT_PATH)/tmp/index.bin
	$(FLASH_TOOL) write_flash $(PART_ADDR) $(PROJECT_PATH)/tmp/index.bin

erase:
	$(FLASH_TOOL) erase_flash

read:
	$(FLASH_TOOL) read_flash $(PART_ADDR) 0x1000 storage.bin

read_nvs:
	$(FLASH_TOOL) read_flash 0x009000 0x006000 nvs.bin
