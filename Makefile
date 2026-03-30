.PHONY: build test download-testdata clean

build:
	cmake -B build -S .
	cmake --build build
	go build ./cmd/same-blue

test:
	./build/Same_Blue_tests
	go test ./...

download-testdata:
	yt-dlp -f "bestvideo+bestaudio" --merge-output-format mp4 \
		-o "data/%(title)s [%(id)s].%(ext)s" \
		"https://www.youtube.com/watch?v=mkggXE5e2yk"

clean:
	rm -rf build same-blue
