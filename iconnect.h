
enum addrmode {
	ADDRMODE_RELATIVE,
	ADDRMODE_ABSOLUTE
};

struct memmap_entry {
	uint64_t addr;
	uint64_t size;
	enum addrmode addrmode;
	int sk_idx;
};

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
class iconnect
: public sc_core::sc_module
{
public:
	struct memmap_entry map[N_TARGETS * 4];

	tlm_utils::simple_target_socket_tagged<iconnect> *t_sk[N_INITIATORS];
	tlm_utils::simple_initiator_socket_tagged<iconnect> *i_sk[N_TARGETS];

	SC_HAS_PROCESS(iconnect);
	iconnect(sc_core::sc_module_name name);
	virtual void b_transport(int id,
				 tlm::tlm_generic_payload& trans,
				 sc_time& delay);


	virtual bool get_direct_mem_ptr(int id,
                                  tlm::tlm_generic_payload& trans,
                                  tlm::tlm_dmi&  dmi_data);

	virtual unsigned int transport_dbg(int id,
					tlm::tlm_generic_payload& trans);

	virtual void invalidate_direct_mem_ptr(int id,
                                         sc_dt::uint64 start_range,
                                         sc_dt::uint64 end_range);

	/*
	 * set_target_offset()
	 *
	 * Used to allow the users to attach an initiator socket
	 * to our target socket that gets all of it's accesses offset by
         * a base before entering the interconnect.  */
	void set_target_offset(int id, sc_dt::uint64 offset);
	int memmap(sc_dt::uint64 addr, sc_dt::uint64 size,
		enum addrmode addrmode, int idx, tlm::tlm_target_socket<> &s);
private:
	sc_dt::int64 target_offset[N_INITIATORS];

	unsigned int map_address(sc_dt::uint64 addr, sc_dt::uint64& offset);
	void unmap_offset(unsigned int target_nr,
				sc_dt::uint64 offset, sc_dt::uint64& addr);

};

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
iconnect<N_INITIATORS, N_TARGETS>::iconnect (sc_module_name name)
	: sc_module(name)
{ //aqui entra lo primero mientras se conecta, cuando se crea el objeto bus de la clase iconnect
	char txt[32];
	unsigned int i;
    cout<<" creacion objeto de clase iconnect: "<<endl;
	for (i = 0; i < N_INITIATORS; i++) {
		sprintf(txt, "target_socket_%d", i);
        cout<<" target_socket___ "<<endl;
		t_sk[i] = new tlm_utils::simple_target_socket_tagged<iconnect>(txt);

		t_sk[i]->register_b_transport(this, &iconnect::b_transport, i);//esto hace que cuando hago devmem, se entre en b_transport
		t_sk[i]->register_transport_dbg(this, &iconnect::transport_dbg, i);
		t_sk[i]->register_get_direct_mem_ptr(this,
				&iconnect::get_direct_mem_ptr, i);
	}

	for (i = 0; i < N_TARGETS; i++) {
		sprintf(txt, "init_socket_%d", i);
        cout<<" init___socket_ "<<endl;
		i_sk[i] = new tlm_utils::simple_initiator_socket_tagged<iconnect>(txt);

		i_sk[i]->register_invalidate_direct_mem_ptr(this,
				&iconnect::invalidate_direct_mem_ptr, i);
		map[i].size = 0;
	}
}

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
void iconnect<N_INITIATORS, N_TARGETS>::set_target_offset(int id,
		sc_dt::uint64 offset)
{   cout<<" set target offset: "<<endl;
	assert(id >= 0 && id < N_INITIATORS);
	target_offset[id] = offset;
}

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
int iconnect<N_INITIATORS, N_TARGETS>::memmap(
		sc_dt::uint64 addr, sc_dt::uint64 size,
		enum addrmode addrmode, int idx,
		tlm::tlm_target_socket<> &s)
{
// aquí entra por que en zynq_demo hay esto:
//bus.memmap, que llama directamente al metodo del objeto bus. Por eso solo usa este metodo y no ningun otro

	unsigned int i;
    cout<<" memmap-iconnect "<<endl;
	for (i = 0; i < N_TARGETS * 4; i++) {
		if (map[i].size == 0) {
			// Found a free entry.  /
			map[i].addr = addr;
			map[i].size = size;
			map[i].addrmode = addrmode;
			map[i].sk_idx = i;
			if (idx == -1)
				i_sk[i]->bind(s);
			else
				map[i].sk_idx = idx;
			return i;
		}
	}
	printf("FATAL! mapping onto full interconnect!\n");
	abort();
	return -1;
}

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
unsigned int iconnect<N_INITIATORS, N_TARGETS>::map_address(
			sc_dt::uint64 addr,
			sc_dt::uint64& offset)
{  //aqui entra con devmem
	unsigned int i;
    cout<<" map address-iconnect: "<<endl;
	for (i = 0; i < N_TARGETS * 4; i++) {
		if (map[i].size
		    && addr >= map[i].addr
		    && addr <= (map[i].addr + map[i].size)) {
			if (map[i].addrmode == ADDRMODE_RELATIVE) {
				offset = addr - map[i].addr;
			} else {
				offset = addr;
			}
			return map[i].sk_idx;
		}
	}

	/* Did not find any slave !?!?  */
        printf("DECODE ERROR-Iconnect! %lx\n", (unsigned long) addr);
	return 0;//quizas aqui tendría que devolver un -1 porque si devuelvo un 0, despues hace la comunicacion mediante el i_sk[0] y por eso devuelve el primer
}

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
void iconnect<N_INITIATORS, N_TARGETS>::unmap_offset(
			unsigned int target_nr,
			sc_dt::uint64 offset,
			sc_dt::uint64& addr)
{ cout<<" unmap offset: "<<endl;
	if (target_nr >= N_TARGETS) {
		SC_REPORT_FATAL("TLM-2", "Invalid target_nr in iconnect\n");
	}

	if (map[target_nr].addrmode == ADDRMODE_RELATIVE) {
		if (offset >= map[target_nr].size) {
			printf("offset=%lx\n", (unsigned long) offset);
			SC_REPORT_FATAL("TLM-2", "Invalid range in iconnect\n");
		}

		addr = map[target_nr].addr + offset;
	} else {
		addr = offset;
	}
	return;
}
////////////////////
////////////////////
template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
void iconnect<N_INITIATORS, N_TARGETS>::b_transport(int id,
			tlm::tlm_generic_payload& trans, sc_time& delay)
{   cout<<endl<<" btransport de iconnect: "<<endl; //aqui entra cada vez q hago devmem
	sc_dt::uint64 addr;

    sc_dt::uint64 addr1;
	sc_dt::uint64 offset;
	unsigned int target_nr;
///////////////////////////////

	tlm::tlm_command cmd = trans.get_command();
	unsigned char *data = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();
	unsigned char *byt = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();

cout<<endl<<" cmd: "<<cmd;
cout<<"  ,len: "<<len;
cout<<"  ,wid: "<<wid;
cout << " &data " <<&data<< endl;

//////////////////////////////////////////////////////////

	if (id >= (int) N_INITIATORS) {
		SC_REPORT_FATAL("TLM-2", "Invalid socket tag in iconnect\n");
	}

	addr = trans.get_address();

printf("addr = trans.get_address-ic(): %lx\n", (unsigned long) addr);
	addr += target_offset[id];
printf("addr += target_offset[id]:-ic: %lx\n", (unsigned long) addr);
    //cout<<" llamada de map addres en btransport1: "<<endl;
	target_nr = map_address(addr, offset); //de aqui voy a map_address, que modifica el offset
    //cout<<" llamada de map addres en btransport2: "<<endl;
trans.set_address(offset);


/*    if(target_nr==3){//esto 3 puede cambiar segun el orden en que realizemos las conexiones en el zynq.//addr==0x3e000000 || addr==0x3f000000 ){ ///target_nr==2
	cout<<" aaddr 3e o 3f"<<endl;
    //trans.set_address(addr);
    addr1 = trans.get_address();
    cout<<" addr1dentroif = trans.get_address(): "<<addr1<<endl;

}else{
trans.set_address(offset);
}*/

	/* Forward the transaction.  */
     //cout<<" Forward the transaction.  "<<endl;
addr1 = trans.get_address();
cout<<" addr1 = trans.get_address(): "<<addr1<<endl;

cout<<" i_sk["<<target_nr<<"]"<<endl;
cout<<" target nr"<<target_nr<<endl;


cout<<"inicia iconnect la comunicacion"<<endl;
	cout << "TRACE1s: " << " "
				     << hex << * (uint32_t *) data<< "\n";

(*i_sk[target_nr])->b_transport(trans, delay); //con esto envío al target, la conexion hecho con el memmap
cout<<"acaba la comunicacion de iconnect"<<endl;
				cout << "TRACE2s: " << " "
				     << hex << * (uint32_t *) data<< "\n";

//en &data deberia estar el dato 
	/* Restore the addresss.  */

    /*esto era para ver si soy capaz de imprimir el valor de data
unsigned char *data7 = trans.get_data_ptr();
    cout<<" data pointer7: "<<&data7<<endl<<endl;
    //cout<<" data7 : "<<data7<<endl<<endl;
    printf("data7a44: %d \n", &data7);*/

	trans.set_address(addr);
    cout<<" salgo de b_transport iconnect "<<endl<<endl<<endl;
/*uint32_t v = 17;
cout<<" v: "<<v<<endl;
memcpy(data, &v, len);*/
}

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
bool iconnect<N_INITIATORS, N_TARGETS>::get_direct_mem_ptr(int id,
					tlm::tlm_generic_payload& trans,
					tlm::tlm_dmi& dmi_data)
{   cout<<" GET DIRECTL MEM PTR "<<endl;
	sc_dt::uint64 addr;
	sc_dt::uint64 offset;
	unsigned int target_nr;
	bool r;

	if (id >= (int) N_INITIATORS) {
		SC_REPORT_FATAL("TLM-2", "Invalid socket tag in iconnect\n");
	}

	addr = trans.get_address();
	addr += target_offset[id];
	target_nr = map_address(addr, offset);

	trans.set_address(offset);
	/* Forward the transaction.  */
	r = (*i_sk[target_nr])->get_direct_mem_ptr(trans, dmi_data);

	unmap_offset(target_nr, dmi_data.get_start_address(), addr);
	dmi_data.set_start_address(addr);
	unmap_offset(target_nr, dmi_data.get_end_address(), addr);
	dmi_data.set_end_address(addr);
	return r;
}

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
unsigned int iconnect<N_INITIATORS, N_TARGETS>::transport_dbg(int id,
				tlm::tlm_generic_payload& trans)
{   cout<<" transport dbg: "<<endl;
	sc_dt::uint64 addr;
	sc_dt::uint64 offset;
	unsigned int target_nr;

	if (id >= (int) N_INITIATORS) {
		SC_REPORT_FATAL("TLM-2", "Invalid socket tag in iconnect\n");
	}

	addr = trans.get_address();
	addr += target_offset[id];
	target_nr = map_address(addr, offset);

	trans.set_address(offset);
	/* Forward the transaction.  */
	(*i_sk[target_nr])->transport_dbg(trans);
	/* Restore the addresss.  */
	trans.set_address(addr);
	return 0;
}

template<unsigned int N_INITIATORS, unsigned int N_TARGETS>
void iconnect<N_INITIATORS, N_TARGETS>::invalidate_direct_mem_ptr(int id,
                                         sc_dt::uint64 start_range,
                                         sc_dt::uint64 end_range)
{   cout<<" invalidate: "<<endl;
	sc_dt::uint64 start, end;
	unsigned int i;

	unmap_offset(id, start_range, start);
	unmap_offset(id, end_range, end);

	/* Reverse the offsetting.  */
	start -= target_offset[id];
	end -= target_offset[id];

	for (i = 0; i < N_INITIATORS; i++) {
		(*t_sk[i])->invalidate_direct_mem_ptr(start, end);
	}
}
