/*
 * A DMA device for demo purposes.
*/

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <inttypes.h>

#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

using namespace sc_core;
using namespace std;

#include "demo-dma.h"
#include <sys/types.h>

demodma::demodma(sc_module_name name)
	: sc_module(name), tgt_socket("tgt-socket")
{   //esto actua cuando se crea la clase
	tgt_socket.register_b_transport(this, &demodma::b_transport);
	memset(&regs, 0, sizeof regs); //memset() se usa para llenar un bloque de memoria con un valor particular. void *memset(void *ptr, int x, size_t n); x es el valor con el que se llena

   /* cout<<" regs.dst_addr: "<<regs.dst_addr<<endl;
    cout<<" regs.src_addr: "<<regs.src_addr<<endl;
    cout<<" regs.len: "<<regs.len<<endl;
    cout<<" regs.ctrl: "<<regs.ctrl<<endl;
    cout<<" regs.byte_en: "<<regs.byte_en<<endl;
    cout<<" regs.error_resp: "<<regs.error_resp<<endl;


    cout<<" &regs.u32[0]: "<<&regs.u32[0]<<endl;
    cout<<" &regs.u32[1]: "<<&regs.u32[1]<<endl;
    cout<<" &regs.u32[2]: "<<&regs.u32[2]<<endl;
    cout<<" &regs.u32[3]: "<<&regs.u32[3]<<endl;
    cout<<" &regs.u32[4]: "<<&regs.u32[4]<<endl;
    cout<<" &regs.u32[5]: "<<&regs.u32[5]<<endl;
    cout<<" &regs.u32[6]: "<<&regs.u32[6]<<endl;
    cout<<" &regs.u32[7]: "<<&regs.u32[7]<<endl;
    cout<<endl;*/

	SC_THREAD(do_dma_copy);
	dont_initialize();
	sensitive << ev_dma_copy;
}

void demodma::do_dma_trans(tlm::tlm_command cmd, unsigned char *buf,
				sc_dt::uint64 addr, sc_dt::uint64 len)
{
	tlm::tlm_generic_payload tr;
	sc_time delay = SC_ZERO_TIME;
        cout<<" tdo_dma_trans ------------"<<endl;
	tr.set_command(cmd);
	tr.set_address(addr);
	tr.set_data_ptr(buf);
	tr.set_data_length(len);
	tr.set_streaming_width(len);
	tr.set_dmi_allowed(false);
	tr.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

cout<<" cmd "<<cmd;//<<" buf "<<buf<<endl;
printf(", buf: %u \n",buf);
printf("addr: %lx\n", (unsigned long) addr);

	if (regs.byte_en) {
		tr.set_byte_enable_ptr((unsigned char *) &regs.byte_en);
		tr.set_byte_enable_length(sizeof regs.byte_en);
	}
    cout<<" inicia el dma "<<endl;
	init_socket->b_transport(tr, delay); //aqui inicia transaccion dma mediante socket initiator
cout<<" se acaba transaccion del dma "<<endl;
	switch (tr.get_response_status()) {
	case tlm::TLM_OK_RESPONSE:
		regs.error_resp = DEMODMA_RESP_OKAY;
		break;
	case tlm::TLM_ADDRESS_ERROR_RESPONSE:
		printf("%s:%d DMA transaction error!\n", __func__, __LINE__);
		regs.error_resp = DEMODMA_RESP_ADDR_DECODE_ERROR;
		break;
	default:
		printf("%s:%d DMA transaction error!\n", __func__, __LINE__);
		regs.error_resp = DEMODMA_RESP_BUS_GENERIC_ERROR;
		break;
	}
}

void demodma::update_irqs(void)
{           cout<<" update_irqs "<<endl;
	irq.write(regs.ctrl & DEMODMA_CTRL_DONE);
}

void demodma::do_dma_copy(void)
{
	unsigned char buf[32];
 cout<<" do_dma_copy"<<endl;
 /*regs.len=37;
 regs.ctrl=1;*/
 cout<<" regs.len: "<<regs.len<<endl;
 cout<<" regs.len: "<<regs.len<<endl;
 cout<<" sizeof buf: "<<sizeof buf<<endl;
 cout<<" DEMODMA_CTRL_RUN: "<<DEMODMA_CTRL_RUN<<endl;

	while (true) {
		if (!(regs.ctrl & DEMODMA_CTRL_RUN)) {
 cout<<" true1"<<endl;
			wait(ev_dma_copy);
		}
 cout<<" true2"<<endl;
		if (regs.len > 0 && regs.ctrl & DEMODMA_CTRL_RUN) {
			unsigned int tlen = regs.len > sizeof buf ? sizeof buf : regs.len;
 cout<<"---- tlen="<<tlen<<endl;
			do_dma_trans(tlm::TLM_READ_COMMAND, buf, regs.src_addr, tlen);
///imprimir buf
			do_dma_trans(tlm::TLM_WRITE_COMMAND, buf, regs.dst_addr, tlen);

			regs.dst_addr += tlen;
			regs.src_addr += tlen;
			regs.len -= tlen;

printf("regs.src_addr %lx\n", (unsigned long) regs.src_addr);
printf("regs.dst_addr %lx\n", (unsigned long) regs.dst_addr);
cout<<" regs.len: "<<regs.len<<endl;
		}
 cout<<" true3"<<endl;
		if (regs.len == 0 && regs.ctrl & DEMODMA_CTRL_RUN) {
			regs.ctrl &= ~DEMODMA_CTRL_RUN;
			/* If the DMA was running, signal done.  */
			regs.ctrl |= DEMODMA_CTRL_DONE;
 cout<<" true4"<<endl;///cuando entra aqui, acaba el bucle infinito y se queda esperando, ya que así cambia el ctrl
		} else {
			// Artificial delay between bursts.
			wait(sc_time(1, SC_US));
 cout<<" true5"<<endl;
		}
		update_irqs();
	}
}

void demodma::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{
	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *data = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();
	unsigned char *byt = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();
    
        cout<<endl<<" b_transport del dma "<<endl;
        
        cout << " &data " <<&data<< endl;
   
	if (byt != 0) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}

	if (len > 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}
cout << " addr " <<addr<< endl;
	addr >>= 2;//desplaza dos a la derecha: de 1100 a 0011
cout << " addr " <<addr<< endl;
	addr &= 7;//hace and entre addr y 0111 por tanto: 0011 & 0111 es 0011
cout << " addr " <<addr<< endl;

	if (trans.get_command() == tlm::TLM_READ_COMMAND) {//siempre enta aqui cuando entra desde iconnect. read significa que paso informacion desde dma a iconnec, write significa que se pasa informacion al dma
        //regs.u32[addr]=16;
uint32_t v = 0;
		memcpy(data, &regs.u32[addr], len);
        cout<<" trans.get_command() == tlm::TLM_READ_COMMAND "<<endl;
      /*  cout<<" addr: "<<addr<<endl;
        cout<<" &regs.u32[addr] "<<&regs.u32[addr]<<endl; //esto es una direccion
        cout<<" regs.u32[addr] "<<regs.u32[addr]<<endl; //esto es 0 y por eso envia un cero y en qemu sale 0x00000*/


	} else if (cmd == tlm::TLM_WRITE_COMMAND) {
        cout<<" cmd == tlm::TLM_WRITE_COMMAND "<<endl;
        cout<<" addr= "<<addr<<endl;
		unsigned char buf[4];
uint32_t v = 0;
	memcpy(&regs.u32[addr], data, len);//esto significa write, pasar datos desde trans al dma(al registro regs).//escribir lo que hay en data en posicion memoria &regs

cout<<" regs.u32[0] "<<regs.u32[0]<<endl;
cout<<" regs.u32[1] "<<regs.u32[1]<<endl;
cout<<" regs.u32[2] "<<regs.u32[2]<<endl;
cout<<" regs.u32[3] "<<regs.u32[3]<<endl;
cout<<" regs.u32[4] "<<regs.u32[4]<<endl;
cout<<" regs.u32[5] "<<regs.u32[5]<<endl;
cout<<" regs.u32[6] "<<regs.u32[6]<<endl;
cout<<" regs.u32[7] "<<regs.u32[7]<<endl;

printf("regs.src_addr %lx\n", (unsigned long) regs.src_addr);
printf("regs.dst_addr %lx\n", (unsigned long) regs.dst_addr);

/*memcpy(&v, data, len);
v=v+2;
//v=0x3e000000;
printf(" v= %lx\n", (unsigned long) v);
memcpy(&regs.u32[addr], &v, len);*/
///////
/*cout<<" &regs.u32[addr] "<<&regs.u32[addr]<<endl;
cout<<" regs.u32[addr] "<<regs.u32[addr]<<endl;*/
            /*regs.src_addr=0x3f000000;
            regs.dst_addr=0x3f000001;*/
/*cout<<" regs.src_addr "<<regs.src_addr<<endl;
cout<<" regs.dst_addr "<<regs.dst_addr<<endl;
printf("regs.src_addr %lx\n", (unsigned long) regs.src_addr);
printf("regs.dst_addr %lx\n", (unsigned long) regs.dst_addr);*/


		switch (addr) {
			case 3:
				// speculative read for testing inline path.
cout<<" 3 case3"<<endl;
				do_dma_trans(tlm::TLM_READ_COMMAND, buf, regs.src_addr, 4);
				/* The dma copies after a usec.  */
cout<<" The dma copies after a usec. "<<endl;
				ev_dma_copy.notify(delay + sc_time(1, SC_US));
cout<<" The dma copies after a usec.------------------- "<<endl;
				break;
///////////////////////
/*            case 2:
unsigned char buf1[32];
printf("2regs.src_addr %lx\n", (unsigned long) regs.src_addr);
printf("2regs.dst_addr %lx\n", (unsigned long) regs.dst_addr);
                do_dma_trans(tlm::TLM_READ_COMMAND, buf1, regs.src_addr, 4);
                do_dma_trans(tlm::TLM_WRITE_COMMAND, buf1, regs.dst_addr, 4);
cout<<" finaliza case2 "<<endl<<endl<<endl<<endl<<endl<<endl<<endl;
                break;*/
/////////////////////
			default:
				/* No side-effect.  */
				break;
		}
	}
	trans.set_response_status(tlm::TLM_OK_RESPONSE);
    cout<<" salgo b_transport de dma "<<endl;
}
