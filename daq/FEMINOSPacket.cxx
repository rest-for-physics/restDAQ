
#include <FEMINOSPacket.h>

#include <cstdio>
#include <inttypes.h>

void FEMINOSPacket::DataPacket_Print (uint16_t *fr, const uint16_t &size){

  int sz_rd=0,si=0;
  uint16_t r0, r1, r2, r3;

  do {
    if ((*fr & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_IX){
      r0 = GET_CARD_IX(*fr);
      r1 = GET_CHIP_IX(*fr);
      r2 = GET_CHAN_IX(*fr);
      printf("Card %02d Chip %01d Channel %02d\n", r0, r1, r2);
      fr++;
      sz_rd++;
      si=0;
    } else if ((*fr & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_CNT){
      r0 = GET_CARD_IX(*fr);
      r1 = GET_CHIP_IX(*fr);
      r2 = GET_CHAN_IX(*fr);
      printf( "Card %02d Chip %01d Channel_Hit_Count %02d\n", r0, r1, r2);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HISTO){
      r0 = GET_CARD_IX(*fr);
      r1 = GET_CHIP_IX(*fr);
      r2 = GET_CHAN_IX(*fr);
      printf( "Card %02d Chip %01d Channel_Histo_Index %02d\n", r0, r1, r2);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE) {
      r0 = GET_ADC_DATA(*fr);
      printf("%03d 0x%04x (%4d)\n", si, r0, r0);
      fr++;
      sz_rd++;
      si++;
    } else if ((*fr & PFX_12_BIT_CONTENT_MASK) == PFX_LAT_HISTO_BIN) {
      r0 = GET_LAT_HISTO_BIN(*fr);
      fr++;
      sz_rd++;
      uint32_t tmp = GetUInt32FromBufferInv(fr, sz_rd);fr+=2;
      printf("%03d %03d\n", r0, tmp);
    } else if ((*fr & PFX_12_BIT_CONTENT_MASK) == PFX_CHIP_LAST_CELL_READ){
        for(int i=0;i<4;i++){
          printf( "Chip %01d Last_Cell_Read %03d (0x%03x)\n",GET_LST_READ_CELL_CHIP_IX(*fr), GET_LST_READ_CELL(*fr), GET_LST_READ_CELL(*fr));
          fr++;
          sz_rd++;
        }
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {
      r0 = GET_TIME_BIN(*fr);
      printf("Time_Bin: %d\n", r0);
      fr++;
      sz_rd++;
      si = 0;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_HISTO_BIN_IX){
      r0 = GET_HISTO_BIN(*fr);
      fr++;
      sz_rd++;
      printf("Bin %3d Val %5d\n", r0, *fr);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_PEDTHR_LIST){
      r0 = GET_PEDTHR_LIST_FEM(*fr);
      r1 = GET_PEDTHR_LIST_ASIC(*fr);
      r2 = GET_PEDTHR_LIST_MODE(*fr);
      r3 = GET_PEDTHR_LIST_TYPE(*fr);
      fr++;
      sz_rd++;

        if (r3 == 0){ // pedestal entry
          printf("# Pedestal List for FEM %02d ASIC %01d\n", r0, r1);
        } else {
          printf("# Threshold List for FEM %02d ASIC %01d\n", r0, r1);
        }
      printf("fem %02d\n", r0);
        if(r2==0){ //AGET
          r2 = 71;
        } else {
          r2 = 78;
        }

        for (int j=0;j<=r2; j++){
            if(r3==0)printf("ped ");
            else printf("thr ");
          printf("%1d %2d 0x%04x (%4d)\n", r1, j, *fr, *fr);
          fr++;
          sz_rd++;
        }
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME){
      r0 = GET_FRAMING_VERSION(*fr);
      r1 = GET_FEMID(*fr);
      fr++;
      sz_rd++;
      printf("--- Start of Data Frame (V.%01d) FEM %02d --\n", r0, r1);
      printf("Filled with %d bytes\n", *fr);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME) {
      r0 = GET_FRAMING_VERSION(*fr);
      r1 = GET_FEMID(*fr);
      printf("--- Start of Moni Frame (V.%01d) FEM %02d --\n", r0, r1);
      fr++;
      sz_rd++;
      printf("Filled with %d bytes\n", *fr);
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_CFRAME) {
      r0 = GET_FRAMING_VERSION(*fr);
      r1 = GET_FEMID(*fr);
      fr++;
      sz_rd++;
      printf("--- Start of Config Frame (V.%01d) FEM %02d --\n", r0, r1);
      printf("Error code: %d\n", *((short *) fr));
      fr++;
      sz_rd++;
    } else if ((*fr & PFX_8_BIT_CONTENT_MASK) == PFX_ASCII_MSG_LEN){
      r0 = GET_ASCII_LEN(*fr);
      fr++;
      sz_rd++;
      printf("ASCII Msg length: %d\n",r0);
        for (int j=0;j<r0/2; j++){
          printf("%c%c", ((*fr) & 0xFF ),((*fr) >> 8) );
          fr++;
          sz_rd++;
        }
        if ((*fr) & 0x1){// Skip the null string parity
          fr++;
          sz_rd++;
        }
    } else if ((*fr & PFX_4_BIT_CONTENT_MASK) == PFX_START_OF_EVENT) {
      r0 = GET_EVENT_TYPE(*fr);
      printf("-- Start of Event (Type %01d) --\n", r0);
      fr++;
      sz_rd++;
      uint64_t ts = *fr;
      fr++;
      sz_rd++;
      ts |= ( (*fr) << 16);
      fr++;
      sz_rd++;
      ts |= ( (*fr) << 24);
      fr++;
      sz_rd++;
      printf("Time %" PRId64 "\n", ts);

      uint32_t evC =  GetUInt32FromBufferInv(fr, sz_rd);fr+=2;
      printf("Event_Count %d\n", evC);

    } else if ((*fr & PFX_4_BIT_CONTENT_MASK) == PFX_END_OF_EVENT){
      uint32_t size_ev = (( GET_EOE_SIZE(*fr)) << 16 ) & 0xFFFF0000;
      fr++;
      sz_rd++;
      size_ev |= *fr & 0xFFFF;
      fr++;
      sz_rd++;
      printf("----- End of Event ----- (size %d bytes)\n", size);
    } else if ((*fr & PFX_2_BIT_CONTENT_MASK) == PFX_CH_HIT_CNT_HISTO) {
      r0 = GET_CH_HIT_CNT_HISTO_CHIP_IX(*fr);
      printf("Channel Hit Count Histogram (ASIC %d)\n", r0);
      fr++;
      sz_rd++;
      //null word
      fr++;
      sz_rd++;

      fr +=HistoStat_Print (fr, sz_rd, r0);
      
    } else if ((*fr & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME) {
      printf("----- End of Frame -----\n");
      fr++;
      sz_rd++;
    } else if ( *fr  == PFX_NULL_CONTENT ) {
      printf("null word (2 bytes)\n");
      fr++;
      sz_rd++;
    } else if ((*fr == PFX_DEADTIME_HSTAT_BINS) || (*fr == PFX_EVPERIOD_HSTAT_BINS)) {
        if (*fr == PFX_DEADTIME_HSTAT_BINS)printf("Dead-time Histogram\n");
        else printf("Inter Event Time Histogram\n");
      fr++;
      sz_rd++;
      //null word
      fr++;
      sz_rd++;
      
      fr +=HistoStat_Print (fr, sz_rd, 0);

    } else if (*fr == PFX_PEDESTAL_HSTAT) {
      printf("\nPedestal Histogram Statistics\n");
      fr++;
      sz_rd++;

      fr+=HistoStat_Print (fr, sz_rd, 0);

    } else if (*fr == PFX_PEDESTAL_H_MD) {
      fr++;
      sz_rd++;

      uint32_t mean = GetUInt32FromBufferInv(fr, sz_rd);fr+=2;
      uint32_t std_dev = GetUInt32FromBufferInv(fr, sz_rd);fr+=2;
      printf("Mean/Std_dev : %.2f  %.2f\n", (float)mean/100., (float)std_dev/100.);

      fr++;
      sz_rd++;

    } else if (*fr == PFX_SHISTO_BINS) {
      printf("Threshold Turn-on curve\n");
      fr++;
      sz_rd++;
        for(int j=0;j<16;j++){
          printf("%d ", *fr);
          fr++;
          sz_rd++;
        }
      printf("\n\n");
    } else if (*fr == PFX_CMD_STATISTICS) {
      fr++;
      sz_rd++;

      uint32_t tmp_i[9];
        for(int j=0;j<9;j++){tmp_i[j] = GetUInt32FromBuffer(fr, sz_rd);fr+=2;}

      printf("Server RX stat: cmd_count=%d daq_req=%d daq_timeout=%d daq_delayed=%d daq_missing=%d cmd_errors=%d\n", tmp_i[0], tmp_i[1], tmp_i[2], tmp_i[3], tmp_i[4], tmp_i[5]);
      printf("Server TX stat: cmd_replies=%d daq_replies=%d daq_replies_resent=%d\n", tmp_i[6], tmp_i[7], tmp_i[8]);
    } else { // No interpretable data
      printf("word(%04d) : 0x%x (%d) unknown data\n", sz_rd, *fr, *fr);
      fr++;
      sz_rd++;
    }

  } while (sz_rd < size);

  if(sz_rd > size)printf("Format error: read %d words but packet size is %d\n", sz_rd, size);

}

int FEMINOSPacket::HistoStat_Print (uint16_t *fr,  int &sz_rd, const uint16_t &hitCount) {

  int length=sz_rd;

  printf("Min Bin    : %d\n", GetUInt32FromBuffer(fr, sz_rd));fr+=2;
  printf("Max Bin    : %d\n", GetUInt32FromBuffer(fr, sz_rd));fr+=2;
  printf("Bin Width  : %d\n", GetUInt32FromBuffer(fr, sz_rd));fr+=2;
  printf("Bin Count  : %d\n", GetUInt32FromBuffer(fr, sz_rd));fr+=2;
  printf("Min Value  : %d\n", GetUInt32FromBuffer(fr, sz_rd));fr+=2;
  printf("Max Value  : %d\n", GetUInt32FromBuffer(fr, sz_rd));fr+=2;
  printf("Mean       : %.2f\n", ((float) GetUInt32FromBuffer(fr, sz_rd)) / 100.0);fr+=2;
  printf("Std Dev    : %.2f\n", ((float) GetUInt32FromBuffer(fr, sz_rd)) / 100.0);fr+=2;
  printf("Entries    : %d\n", GetUInt32FromBuffer(fr, sz_rd));fr+=2;
    // Get all bins
    for (int j=0; j<hitCount; j++){
      printf("Bin(%2d) = %9d\n", j, GetUInt32FromBuffer(fr, sz_rd));fr+=2;
    }

  length -= sz_rd;
  return length;
}

uint32_t FEMINOSPacket::GetUInt32FromBuffer(uint16_t *fr, int & sz_rd){
  
  uint32_t res = (*fr) << 16;
  fr++;
  sz_rd++;
  res |= *fr;
  fr++;
  sz_rd++;

  return res;
}

uint32_t FEMINOSPacket::GetUInt32FromBufferInv(uint16_t *fr, int & sz_rd){
  
  uint32_t res = *fr;
  fr++;
  sz_rd++;
  res |= (*fr) << 16;
  fr++;
  sz_rd++;

  return res;
}

bool FEMINOSPacket::GetNextEvent(std::deque <uint16_t> &buffer, TRestRawSignalEvent* sEvent, uint64_t &tS, uint32_t &ev_count){

  bool endOfEvent = false;
  int physChannel;
  //std::cout<<__PRETTY_FUNCTION__<<"  "<<buffer.size()<<std::endl;
  while (!endOfEvent && !buffer.empty()){
    //std::cout<<"Val 0x"<<std::hex<<buffer.front()<<std::dec<<" " <<buffer.front()<<std::endl;

    if ((buffer.front() & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME){
      buffer.pop_front();
      //std::cout<<"START OF DFRAME filled with "<< buffer.front() <<" bytes"<<std::endl;
      buffer.pop_front();
    } else if ((buffer.front() & PFX_4_BIT_CONTENT_MASK) == PFX_START_OF_EVENT){
      //std::cout<<"START OF EVENT "<<std::endl;
      buffer.pop_front();
      tS = buffer.front() & 0xFFFF;
      buffer.pop_front();
      tS |= ( buffer.front() << 16) & 0xFFFF0000;
      buffer.pop_front();
      tS |= ( buffer.front() << 24) & 0xFFFF00000000;
      buffer.pop_front();

      //Event count
      ev_count = buffer.front();
      buffer.pop_front();
      ev_count |=  ( buffer.front() << 16);
      buffer.pop_front();

      //std::cout<<"EvCnt "<<ev_count<<" TS "<<tS <<std::endl;
    } else if ((buffer.front() & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_CNT){
      //printf( "Card %02d Chip %01d Channel_Hit_Count %02d\n", GET_CARD_IX(buffer.front()), GET_CHIP_IX(buffer.front()), GET_CHAN_IX(buffer.front()) );
      buffer.pop_front();
    } else if ((buffer.front() & PFX_12_BIT_CONTENT_MASK) == PFX_CHIP_LAST_CELL_READ){
      buffer.pop_front();
    } else if ( (buffer.front() & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_IX ) {
      uint16_t cardID = GET_CARD_IX(buffer.front());
      uint16_t chipID = GET_CHIP_IX(buffer.front());
      uint16_t chID = GET_CHAN_IX(buffer.front());
      physChannel = chID + chipID*72 + cardID*288;
      //std::cout<<" Card "<<cardID<<" Chip "<<chipID<<" Channel "<<chID<<" PhysChann "<<physChannel<<std::endl;
      buffer.pop_front();
      int timeBin=0;
      std::vector<Short_t> sData(512,0);
        while( (buffer.front() & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE ||
               (buffer.front() & PFX_9_BIT_CONTENT_MASK ) == PFX_TIME_BIN_IX){

            if ((buffer.front() & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {
              timeBin = GET_TIME_BIN(buffer.front());
            } else if ((buffer.front() & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE) {
              if(timeBin<512)sData[timeBin] = std::move(GET_ADC_DATA(buffer.front()));
              timeBin++;
              //std::cout<<"TimeBin "<<timeBin<<" "<<GET_ADC_DATA(buffer.front())<<std::endl;
            }
          buffer.pop_front();
          if(buffer.empty())break;
        }

      TRestRawSignal rawSignal(physChannel, sData);
      sEvent->AddSignal(rawSignal);

    } else if ( (buffer.front() & PFX_4_BIT_CONTENT_MASK) == PFX_END_OF_EVENT ){
      endOfEvent=true;
      //std::cout<<"END OF EVENT "<<std::endl;
      buffer.pop_front();
      //std::cout<<" event size "<<buffer.front()<<std::endl;
      buffer.pop_front();//Skip event size
      break;
    } else if ( (buffer.front() & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME ){
      //std::cout<<" END OF FRAME "<<std::endl;
      buffer.pop_front();
      break;
    } else {
       if(buffer.front() !=0)
         printf("WARNING: word : 0x%x (%d) unknown data\n", buffer.front(),buffer.front());
      buffer.pop_front();
    }
  }
  //std::cout<<__PRETTY_FUNCTION__<<" END "<<buffer.size()<<std::endl;
  return endOfEvent;

}

bool FEMINOSPacket::isDataFrame(uint16_t *fr){

  return ((*fr & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME);

}


