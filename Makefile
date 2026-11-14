.PHONY: all shellui embed payload verify clean

SHELLUI_PRX := shellui/bin/xxfwilson23_shellui_metadata_collector.prx
PAYLOAD_BIN := payload/shellui_metadata_loader.bin
PAYLOAD_MAP := payload/shellui_metadata_loader.map
SHA_MANIFEST := artifact-sha256.txt
RAINBOW_METADATA_READY ?= 0
RAINBOW_COLLECTOR ?= 1

all: shellui embed payload verify

shellui:
	$(MAKE) -C shellui RAINBOW_METADATA_READY=$(RAINBOW_METADATA_READY) \
		RAINBOW_COLLECTOR=$(RAINBOW_COLLECTOR)

embed: $(SHELLUI_PRX)
	python3 tools/embed_prx.py --input $(SHELLUI_PRX) \
		--output payload/source/embedded_shellui_prx.c \
		--manifest build-manifest.json

payload: embed
	$(MAKE) -C payload RAINBOW_METADATA_READY=$(RAINBOW_METADATA_READY) \
		RAINBOW_COLLECTOR=$(RAINBOW_COLLECTOR)

verify: $(PAYLOAD_BIN)
	python3 tools/verify_artifacts.py --prx $(SHELLUI_PRX) \
		--payload $(PAYLOAD_BIN) --map $(PAYLOAD_MAP) \
		--manifest build-manifest.json --sha-manifest $(SHA_MANIFEST)

clean:
	$(MAKE) -C shellui clean
	$(MAKE) -C payload clean
	rm -f payload/source/embedded_shellui_prx.c build-manifest.json \
		$(SHA_MANIFEST)
