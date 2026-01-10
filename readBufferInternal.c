typedef struct  {
    int type;
    int lba;
    int sector_cnt;
    unsigned char pad0[8];
    int some_val;
    unsigned char pad1[40];
} readBufferInternal_send_t;

typedef struct  {
    int status;
    unsigned char pad[60];
    unsigned char data[0x40800];
} readBufferInternal_recv_t;

readBufferInternal_send_t readBufferInternal_sendbuf;
readBufferInternal_recv_t readBufferInternal_recvbuf;

static int _fail = 0;
static int _status = 0;

void *readBufferInternal_client;

int readBufferInternal(char *path, int type, int lba, unsigned char *buffer, unsigned int sector_cnt, int some_val) {
    int buffer_off = 0;
    unsigned char retry_cnt = 255;
    
    if (_fail == 1) {
        return -9;
    }

    memset(&readBufferInternal_sendbuf, 0, sizeof(readBufferInternal_sendbuf));
    memset(&readBufferInternal_recvbuf, 0, sizeof(readBufferInternal_recvbuf));
    
    readBufferInternal_sendbuf.type = type;
    readBufferInternal_sendbuf.some_val = some_val;
    
    if (sector_cnt != 0) {
        do {
            int call_rpc_status = 0;
            unsigned int read_sector_cnt = (sector_cnt < 0x81) ? sector_cnt : 0x80;
            _status = 4;
            readBufferInternal_sendbuf.lba = lba;
            readBufferInternal_sendbuf.sector_cnt = read_sector_cnt;
            do {
                if (--retry_cnt == 255) break;

                call_rpc_status = sceSifCallRpc(
                    readBufferInternal_client,
                    0,
                    1,
                    &readBufferInternal_sendbuf,
                    sizeof(readBufferInternal_sendbuf),
                    &readBufferInternal_recvbuf,
                    sizeof(readBufferInternal_recvbuf),
                    0
                );
            } while (call_rpc_status < 0);

            if (call_rpc_status < 0 && retry_cnt == 0) {
                _status = 0;
                return -1;
            }

            while (sceSifCheckStatRpc(readBufferInternal_client));

            _status = 0;

            SyncDCache(
                &readBufferInternal_recvbuf,
                (char *)&readBufferInternal_recvbuf + 0x408400 // Sony wtf? It should of been 0x40840
            );
            if (readBufferInternal_recvbuf.status != 0) {
                if (readBufferInternal_recvbuf.status != -9) {
                    return -2;
                }
                _fail = 1;
                return -9;
            }
            sector_cnt -= read_sector_cnt;
            
            memcpy(buffer + buffer_off, readBufferInternal_recvbuf.data, read_sector_cnt * 0x800);
            
            buffer_off += read_sector_cnt * 0x800;
            lba += read_sector_cnt;
        } while (sector_cnt != 0);
    }
    buffer[buffer_off] = 0; // why NULL termination?
    return 0;
}
