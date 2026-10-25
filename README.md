# Huffman Compressor (C engine + Flask backend + web frontend)

```
huffman-app/
  huff.c            C program: Huffman compress / decompress + analysis (does the real work)
  app.py            Flask server: runs ./huff and serves the page
  static/index.html Frontend: upload, stats, frequency table, Huffman tree drawing
  sample_input.txt  Test file
  requirements.txt
```

## Run
Needs Python 3 and gcc (Windows: MinGW-w64 or WSL).

```
pip install -r requirements.txt
python app.py
```
Open http://127.0.0.1:5000

`app.py` compiles `huff.c` automatically on first run.

## Use the C program alone (no web)
```
gcc huff.c -o huff -lm
./huff c sample_input.txt out.huf
./huff d out.huf restored.txt
```

## How it works
1. Browser uploads the .txt file to `/api/compress`.
2. Flask saves it under a random name and runs `./huff c in.txt out.huf --json`.
3. The C program counts character frequencies, builds the Huffman tree, writes the packed bits
   and prints the analysis (sizes, codes, entropy, tree) as JSON.
4. The page draws the tables, the size bars and the tree. `.huf` can be downloaded
   and uploaded again to `/api/decompress`.

## .huf file format
`"HUF1"` | total chars (int) | unique chars (int) | (char, freq) pairs | packed bits
