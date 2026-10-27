#include <stdio.h>

int text[100000];
int bits[1000000];
int freq[1000];
int left[1000],right[1000];
int parent[1000];
int isright[1000];
int done[1000];
char code[500][100];
int codelen[500];
int order[500];

int main() {
    int n=0;
    int c, i, j, temp;
    int unique=0;
    int nodes =500;
    int root=0;
    c = getchar();
    while(c != EOF) {
        if(n == 100000) {
            printf("file too big");
            return 0;
        }
        text[n] = c;
        n++;
        freq[c]++;
        c=getchar();
    }
    if(n==0) {
        printf("file is emprty:");
        return 0;
    }

    for(i=0; i<256; i++) {
        if(freq[i]>0) {
            unique++;
            root = i;
        }
    }

    for(j = 1; j < unique; j++) {
        int a=-1, b = -1;
        for(i=0; i<nodes; i++) {
            if(done[i] == 1 || freq[i] == 0)
                continue;
            if(a == -1 || freq[i] < freq[a]) {
                b = a;
                a = i;
            }
            else if(b == -1 || freq[i]<freq[b]) {
                b=i;
            }
        }
        freq[nodes] = freq[a] + freq[b];
        left[nodes]=a;
        right[nodes] = b;
        parent[a] = nodes;
        parent[b]=nodes;
        isright[b]=1;
        done[a] = 1;
        done[b]=1;
        root=nodes;
        nodes++;
    }
    long long totalbits = 0;
    int k=0;

    for(i = 0; i < 256; i++) {
        if(freq[i]==0)
            continue;
        char temp2[40];
        int len=0;
        int node = i;
        while(node != root) {
            if(isright[node] == 1)
                temp2[len] = '1';
            else
                temp2[len]='0';
            len++;
            node = parent[node];
        }
        if(len == 0) {
            temp2[0]='0';
            len = 1;
        }
        for(j=0; j<len; j++) {
            code[i][j] = temp2[len - 1 - j];
        }
        code[i][len]='\0';
        codelen[i] = len;
        totalbits = totalbits + (long long)freq[i] * len;
        order[k]=i;
        k++;
    }

    if(totalbits > 1000000) {
        printf("file data too much");
        return 0;
    }

    for(i=0; i<unique-1; i++) {
        for(j = 0; j < unique-1-i; j++) {
            if(freq[order[j]] < freq[order[j+1]]) {
                temp=order[j];
                order[j] = order[j+1];
                order[j+1]=temp;
            }
        }
    }

    int nbits=0;
    for(i = 0; i < n; i++) {
        for(j=0; j<codelen[text[i]]; j++) {
            bits[nbits] = code[text[i]][j] - '0';
            nbits++;
        }
    }

    int ok=1;
    int node = root;
    int pos=0;
    if(unique > 1) {
        for(i=0; i<nbits; i++) {
            if(bits[i] == 0)
                node = left[node];
            else
                node=right[node];
            if(node < 256) {
                if(pos >= n || text[pos] != node)
                    ok=0;
                pos++;
                node = root;
            }
        }

        if(pos != n)
            ok = 0;
    }
    printf("Total characters: %d\n", n);
    printf("Unique characters: %d\n\n",unique);
    for(i=0; i<unique; i++) {
        c = order[i];
        if(c == 32)
            printf("space");
        else if(c == 10)
            printf("\\n ");
        else if(c==9)
            printf("\\t ");
        else if(c > 32 && c < 127)
            printf("%c  ",c);
        else
            printf("(%d)", c);
        printf(" %7d %s\n",freq[c],code[c]);
    }
    long long original = (long long)n * 8;
    long long compressed = (long long)nbits;
    long long tablebytes = (long long)unique * 5;

    printf("Compression details\n");
    printf("Before compression: %d bytes\n", n);
    printf("After compression: %lld bytes\n", (compressed + 7) / 8);
    printf("Bits used: %lld\n", compressed);
    printf("Average per char: %.2f bits\n", (double)compressed / n);
    printf("Table size: %lld bytes\n", tablebytes);
    printf("Total with table: %lld bytes\n", (compressed + 7) / 8 + tablebytes);
    return 0;
}
