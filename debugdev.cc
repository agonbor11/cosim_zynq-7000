/*
 * A demo/debug device.
 *
 */

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <inttypes.h>

#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

using namespace sc_core;
using namespace std;

#include "debugdev.h"
#include <sys/types.h>
#include <time.h>

debugdev::debugdev(sc_module_name name)
	: sc_module(name), socket("socket")
{   cout<<" debug.dev.cc aqui entra al principio cuando se crea el objeto debug de esta clase en zynq_demo.cc"<<endl<<endl<<endl<<endl<<endl<<endl<<endl<<endl;
	socket.register_b_transport(this, &debugdev::b_transport);
	socket.register_transport_dbg(this, &debugdev::transport_dbg);
}


void debugdev::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{   
	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *data = trans.get_data_ptr(); //transmite donde está el puntero del dato que se envia, siempre es el mismo dentro de la misma simulación

  cout<<" b_transport dentro de debugdev"<<endl<<endl;
 printf("addr %lx\n", (unsigned long) addr);
   cout<<" data pointer: "<<&data<<endl<<endl;
	unsigned int len = trans.get_data_length(); //transmite la longitud del dato
    //cout<<" lenght"<<len<<endl<<endl;
	unsigned char *byt = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();
   
	if (byt != 0) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}

	if (len > 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}
	trans.set_response_status(tlm::TLM_OK_RESPONSE);

	// Pretend this is slow!
	delay += sc_time(1, SC_US);

	if (trans.get_command() == tlm::TLM_READ_COMMAND) {
		sc_time now = sc_time_stamp() + delay;
		uint32_t v = 0; //siempre entra aqui y addr es 0
   // cout<<" dentro del if"<<endl; //siempre entro aqui
    //cout<<" addr "<<addr<<endl;
		switch (addr) {
			case 0:
//				cout << "read " << addr << " " << t << endl; 
				v = now.to_seconds() * 1000 * 1000 * 1000;
                //siempre entra aquí y v son los segundos
				break;
			case 0xc:
				v = irq.read();
				break;
			case 0x10:
				v = clock();
				break;
			case 0xf0:
				trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
				break;
			case 0xf4:
				trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
				break;
			default:
				break;
		}
		memcpy(data, &v, len);//aqui se vuelca la información de &v a data(q es puntero)(no se donde ni como ver el data)

        /* cout<<"data: "<<data<<" v: "<<v<<" &v: "<<&v<<endl;
    cout<<" data pointer: "<<&data<<endl<<endl;
     cout<<" data:  "<<+data<<endl<<endl;

        printf("arriba data75555   :  %u  \n", data);*/
	} else if (cmd == tlm::TLM_WRITE_COMMAND) {
		static sc_time old_ts = SC_ZERO_TIME, now, diff;
    cout<<" dentro del elseif"<<endl<<endl;
		now = sc_time_stamp() + delay;
		diff = now - old_ts;
		switch (addr) {
			case 0:
				cout << "TRACE: " << " "
				     << hex << * (uint32_t *) data
				     << " " << now << " diff=" << diff << "\n";
				break;
			case 0x4:
				putchar(* (uint32_t *) data);
				break;
			case 0x8:
				cout << "STOP: " << " "
				     << hex << * (uint32_t *) data
				     << " " << now << "\n";
				sc_stop();
				exit(1);
				break;
			case 0xc:
				irq.write(data[0] & 1);
				break;
			case 0xf0:
				trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
				break;
			case 0xf4:
				trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
				break;
			default:
				break;
		}
		old_ts = now;

	}
    cout<<" salgo de debugdev"<<endl<<endl;
}

unsigned int debugdev::transport_dbg(tlm::tlm_generic_payload& trans)
{ 
	unsigned int len = trans.get_data_length();
	return len;
}
