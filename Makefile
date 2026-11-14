.PHONY: all shellui embed payload verify clean
.NOTPARALLEL: all

SHELLUI_PRX := shellui/bin/xxfwilson23_rainbow_shellui.prx
PAYLOAD_BIN := payload/rainbow_loader.bin
PAYLOAD_MAP := payload/rainbow_loader.map
SHA_MANIFEST := artifact-sha256.txt

all: shellui embed payload verify

shellui:
	$(MAKE) -C shellui

embed: shellui
	python3 tools/embed_prx.py --input $(SHELLUI_PRX) \
		--output payload/source/embedded_shellui_prx.c \
		--manifest build-manifest.json

payload: embed
	$(MAKE) -C payload

verify:
	python3 tools/verify_artifacts.py --prx $(SHELLUI_PRX) \
		--payload $(PAYLOAD_BIN) --map $(PAYLOAD_MAP) \
		--manifest build-manifest.json --sha-manifest $(SHA_MANIFEST)

clean:
	$(MAKE) -C shellui clean
	$(MAKE) -C payload clean
	rm -f payload/source/embedded_shellui_prx.c build-manifest.json \
		$(SHA_MANIFEST)
