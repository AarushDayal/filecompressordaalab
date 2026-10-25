/* Simple Huffman compressor for .txt files
 * Build : gcc huff.c -o huff -lm
 * Use   : ./huff c input.txt output.huf [--json]
 *         ./huff d input.huf output.txt [--json]
 * --json prints machine-readable output (used by the web app).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

int freq[512];                       /* 0-255 = characters, 256+ = merged nodes */
int left[512], right[512], parent[512], isright[512], done[512];
int nodes, root, total;              /* total = number of characters in file */
int json = 0;                        /* 1 = print JSON instead of a text table */

void fail(char *msg) {
    if (json) printf("{\"error\":\"%s\"}\n", msg);
    else      printf("%s\n", msg);
}

/* Repeatedly merge the 2 smallest nodes until one tree remains */
void build_tree() {
    int count = 0, i;
    for (i = 0; i < 256; i++) if (freq[i] > 0) { count++; root = i; }
    nodes = 256;
    for (int k = 1; k < count; k++) {
        int a = -1, b = -1;                      /* a = smallest, b = 2nd smallest */
        for (i = 0; i < nodes; i++) {
            if (done[i] || freq[i] == 0) continue;
            if (a == -1 || freq[i] < freq[a]) { b = a; a = i; }
            else if (b == -1 || freq[i] < freq[b]) b = i;
        }
        freq[nodes] = freq[a] + freq[b];
        left[nodes] = a;  right[nodes] = b;
        parent[a] = parent[b] = nodes;
        isright[b] = 1;
        done[a] = done[b] = 1;
        root = nodes++;
    }
}

/* print the tree as nested JSON (for the frontend tree drawing) */
void print_tree(int n) {
    if (n < 256) { printf("{\"f\":%d,\"c\":%d}", freq[n], n); return; }
    printf("{\"f\":%d,\"l\":", freq[n]);  print_tree(left[n]);
    printf(",\"r\":");                    print_tree(right[n]);
    printf("}");
}

void compress(char *inname, char *outname) {
    int len = strlen(inname);
    if (len < 5 || strcmp(inname + len - 4, ".txt") != 0) {
        fail("Only .txt files are allowed!");
        return;
    }
    FILE *in = fopen(inname, "rb");
    FILE *out = fopen(outname, "wb");
    if (!in || !out) { fail("File error"); return; }

    /* 1. count frequency of every character */
    int c;
    while ((c = fgetc(in)) != EOF) { freq[c]++; total++; }
    if (total == 0) { fail("File is empty"); return; }

    /* 2. build tree */
    build_tree();

    /* 3. make code for each character (walk from leaf up to root) */
    char code[256][300];
    int used = 0, i;
    long long bits = 0;
    double entropy = 0;
    for (i = 0; i < 256; i++) {
        if (freq[i] == 0) continue;
        used++;
        char tmp[300];
        int n = 0, node = i;
        while (node != root) { tmp[n++] = isright[node] ? '1' : '0'; node = parent[node]; }
        if (n == 0) tmp[n++] = '0';              /* file has only 1 unique char */
        for (int j = 0; j < n; j++) code[i][j] = tmp[n - 1 - j];   /* reverse */
        code[i][n] = '\0';
        bits += (long long)freq[i] * n;
        double p = (double)freq[i] / total;
        entropy -= p * log2(p);
    }

    /* 4. header: "HUF1", total, number of unique chars, then (char, freq) pairs */
    fwrite("HUF1", 1, 4, out);
    fwrite(&total, sizeof(int), 1, out);
    fwrite(&used, sizeof(int), 1, out);
    for (i = 0; i < 256; i++)
        if (freq[i] > 0) { fputc(i, out); fwrite(&freq[i], sizeof(int), 1, out); }

    /* 5. write the compressed bits, packed 8 per byte */
    rewind(in);
    int cur = 0, nbits = 0;
    while ((c = fgetc(in)) != EOF)
        for (char *p = code[c]; *p; p++) {
            cur = cur * 2 + (*p - '0');
            if (++nbits == 8) { fputc(cur, out); cur = 0; nbits = 0; }
        }
    if (nbits > 0) fputc(cur << (8 - nbits), out);   /* pad last byte with 0s */
    long outsize = ftell(out);
    fclose(in); fclose(out);

    /* 6. analysis */
    int hdr = 12 + used * 5;
    double avg = (double)bits / total;
    double saved = 100.0 * (total - outsize) / total;

    if (json) {
        printf("{\"original\":%d,\"compressed\":%ld,\"header\":%d,\"unique\":%d,", total, outsize, hdr, used);
        printf("\"saved\":%.2f,\"avg_bits\":%.4f,\"entropy\":%.4f,\"symbols\":[", saved, avg, entropy);
        int first = 1;
        for (i = 0; i < 256; i++) {
            if (freq[i] == 0) continue;
            printf("%s{\"ch\":%d,\"count\":%d,\"code\":\"%s\"}", first ? "" : ",", i, freq[i], code[i]);
            first = 0;
        }
        printf("],\"tree\":");
        print_tree(root);
        printf("}\n");
        return;
    }

    printf("\nChar    Count    Freq%%   Code\n");
    printf("-------------------------------------\n");
    for (i = 0; i < 256; i++) {
        if (freq[i] == 0) continue;
        if (i > 32 && i < 127) printf("'%c'   ", i);
        else                   printf("(%3d)", i);
        printf("  %7d  %6.2f%%   %s\n", freq[i], 100.0 * freq[i] / total, code[i]);
    }
    printf("\nUnique characters : %d\n", used);
    printf("Original size     : %d bytes\n", total);
    printf("Compressed size   : %ld bytes (header %d + data %ld)\n", outsize, hdr, outsize - hdr);
    printf("Space saved       : %.2f%%\n", saved);
    printf("Avg bits per char : %.3f (normal = 8)\n", avg);
    printf("Entropy           : %.3f bits/char (best possible)\n", entropy);
    printf("Saved to          : %s\n", outname);
}

void decompress(char *inname, char *outname) {
    FILE *in = fopen(inname, "rb");
    FILE *out = fopen(outname, "wb");
    if (!in || !out) { fail("File error"); return; }

    /* read header and rebuild the same tree */
    char magic[4];
    int used, f, c, sum = 0;
    if (fread(magic, 1, 4, in) != 4 || memcmp(magic, "HUF1", 4) != 0) {
        fail("Not a valid .huf file"); return;
    }
    if (fread(&total, sizeof(int), 1, in) != 1 || fread(&used, sizeof(int), 1, in) != 1
        || used < 1 || used > 256 || total < 1) {
        fail("Corrupt .huf header"); return;
    }
    for (int i = 0; i < used; i++) {
        c = fgetc(in);
        if (c == EOF || fread(&f, sizeof(int), 1, in) != 1 || f < 1) {
            fail("Corrupt .huf header"); return;
        }
        freq[c] = f;
        sum += f;
    }
    if (sum != total) { fail("Corrupt .huf header"); return; }
    build_tree();

    /* read bits: 0 = go left, 1 = go right, reaching a leaf (<256) = got a char */
    int written = 0, node = root;
    if (used == 1) {
        for (; written < total; written++) fputc(root, out);
    }
    while (written < total && (c = fgetc(in)) != EOF)
        for (int b = 7; b >= 0 && written < total; b--) {
            node = ((c >> b) & 1) ? right[node] : left[node];
            if (node < 256) { fputc(node, out); written++; node = root; }
        }
    fclose(in); fclose(out);

    if (written != total) { fail("File is truncated or corrupt"); return; }
    if (json) printf("{\"ok\":true,\"chars\":%d}\n", written);
    else      printf("Decompressed %d characters to %s\n", written, outname);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage:\n  %s c input.txt output.huf [--json]\n  %s d input.huf output.txt [--json]\n", argv[0], argv[0]);
        return 1;
    }
    json = (argc >= 5 && strcmp(argv[4], "--json") == 0);
    if (argv[1][0] == 'c') compress(argv[2], argv[3]);
    else                   decompress(argv[2], argv[3]);
    return 0;
}
