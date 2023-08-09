import zstandard as zstd

input_path = input("Input path: ")
output_path = "output.pgn"

dctx = zstd.ZstdDecompressor()
with open(input_path, 'rb') as ifh, open(output_path, 'wb') as ofh:
    dctx.copy_stream(ifh, ofh)