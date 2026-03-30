.PHONY: build test download-testdata install-yt-dlp clean

build:
	cmake -B build -S .
	cmake --build build
	go build ./cmd/same-blue

test:
	./build/Same_Blue_tests
	go test ./...

install-yt-dlp:
ifeq ($(OS),Windows_NT)
	winget install yt-dlp
else
	brew install yt-dlp
endif

URL ?= https://www.youtube.com/watch?v=mkggXE5e2yk

download-testdata:
	yt-dlp -f "bestvideo+bestaudio" --merge-output-format mp4 \
		-o "data/%(title)s [%(id)s].%(ext)s" \
		"$(URL)"

clean:
	rm -rf build same-blue
