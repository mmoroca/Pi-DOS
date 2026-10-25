.PHONY: help fetch sdl2 phase0 smoke smoke-programs smoke-graphics smoke-keyboard smoke-audio

help:
	@echo 'make image-config - configure the shared Raspberry Pi ARM64 image'
	@echo 'make image - build the Raspberry Pi ARM64 SD image (JOBS=4 by default)'
	@echo 'make fetch   - fetch and verify pinned Phase 0 sources'
	@echo 'make sdl2    - build private native SDL2 with required backends'
	@echo 'make phase0  - build native SDL2 and DOSBox-X (JOBS=4 by default)'
	@echo 'make smoke   - check DOS shell startup headlessly with isolated config'
	@echo 'make smoke-programs - check COM/EXE/BAT execution (requires NASM)'
	@echo 'make smoke-graphics - verify GLES VGA pattern and KMS flips (requires free HDMI display)'
	@echo 'make smoke-keyboard - verify uinput/SDL2/DOS keyboard path (Pi, root, free HDMI)'
	@echo 'make smoke-audio - verify DOS tones and ALSA HDMI playback (Pi, free HDMI/audio)'
	@echo 'Offline build: bash tools/phase0/build.sh --offline'

fetch:
	python3 tools/phase0/fetch.py

sdl2:
	bash tools/phase0/build.sh --sdl-only

phase0:
	bash tools/phase0/build.sh

smoke:
	bash tools/phase0/smoke.sh

smoke-programs:
	bash tools/phase0/programs.sh

smoke-graphics:
	python3 tools/phase0/graphics.py

smoke-keyboard:
	python3 tools/phase0/keyboard.py

smoke-audio:
	python3 tools/phase0/audio.py

.PHONY: image-config image
image-config:
	bash tools/image/build.sh configure

image:
	bash tools/image/build.sh build
