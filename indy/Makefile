# Gates for the SGI Indy port. Run inside the devshell: nix develop
#
#   make m0           build the M0 test images
#   make gate-m0      serial gate with the M0 image
#   make gate-gfx-m0  graphics gate with the M0 image
#   make gate-rex3    graphics gate with the REX3 upload test
#   make netboot IMAGE=FILE   copy an image to the TFTP root of the real Indy

OUT = ../generated.mips
M0 = $(OUT)/m0
NETBOOT_DIR ?= /var/lib/sgi-netboot

m0:
	$(MAKE) -C m0 OUT=../$(M0)

gate-m0: m0
	./gate.sh $(M0)/hello.ecoff "haiku: m0 alive" 60

gate-gfx-m0: m0
	./gate-gfx.sh $(M0)/hello.ecoff "haiku: m0 alive" $(OUT)/m0-gfx.png 120

gate-rex3: m0
	./gate-gfx.sh $(M0)/rex3.ecoff "haiku: m0 rex3 done" $(OUT)/m0-rex3.png 120

netboot:
	test -n "$(IMAGE)"
	cp $(IMAGE) $(NETBOOT_DIR)/haiku

.PHONY: m0 gate-m0 gate-gfx-m0 gate-rex3 netboot
