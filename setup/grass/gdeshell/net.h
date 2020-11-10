
// net.h
// 


#ifndef __NET_H
#define __NET_H    1



#define ETH_TYPE_IP     0x0800  
#define ETH_TYPE_ARP    0x0806
 
 
#define ARP_OPC_REQUEST  0x01
#define ARP_OPC_REPLY    0x02



// little endian
#define ToNetByteOrder16(v)   ((v >> 8) | (v << 8))
#define ToNetByteOrder32(v)   (((v >> 24) & 0xFF) | ((v << 8) & 0xFF0000) | ((v >> 8) & 0xFF00) | ((v << 24) & 0xFF000000))
#define FromNetByteOrder16(v) ((v >> 8) | (v << 8))
#define FromNetByteOrder32(v) (((v >> 24) & 0xFF) | ((v << 8) & 0xFF0000) | ((v >> 8) & 0xFF00) | ((v << 24) & 0xFF000000))

//extra
#define gdeshell_FromNetByteOrder16(v) ((v >> 8) | (v << 8))



//
// == Ethernet ==============================================
//

// Ethernet header length
#define ETHERNET_HEADER_LENGHT  14  

// ethernet header
struct gdeshell_ether_header 
{
    // mac
    uint8_t dst[6];
    uint8_t src[6];

    // protocol
    uint16_t type;

} __attribute__((packed)); 



//
// == ARP =================================================
//


// ARP header length
#define ARP_HEADER_LENGHT 28      

// arp struct
struct  gdeshell_ether_arp {

    uint16_t type;  //Hardware type (HTYPE)
    uint16_t proto; //Protocol type (PTYPE)
    uint8_t hlen;   //Hardware address length 
    uint8_t plen;   //Protocol address length 
    uint16_t op;    //Operation (OPER)

   //#define ETH_ALEN 6 

    uint8_t arp_sha[6];    /* sender hardware address mac */
    uint8_t arp_spa[4];    /* sender protocol address ip */

    uint8_t arp_tha[6];    /* target hardware address mac*/
    uint8_t arp_tpa[4];    /* target protocol address ip*/

} __attribute__((packed)); 




//
// == Prototypes ==============================================
//

// Send ARP.
void 
__SendARP ( 
    uint8_t src_ip[4], 
    uint8_t dst_ip[4],
    uint8_t src_mac[6], 
    uint8_t dst_mac[6] );

// Test ARP.
void __shellTestARP (void);


// Initialize the network low level support.
void network_initialize (void);

// Send packet.
// Direto. Envia o endereço do pacote para o kernel.
// So lembrando que o lernel nao fara procesamentos
// de protocolos de rede.

void gdeshell_send_packet( unsigned long packet_buffer );

void 
print_ethernet_header ( 
    const unsigned char *Buffer, 
    int Size );

void print_arp_header ( char *Buffer );

int 
gdeshell_decode_buffer ( unsigned long buffer_address );

void network_test_buffer(void);

// Testa rotinas do kernel.
// Nao eh socket unix.
void shellSocketTest (void);


#endif    




