#include "predictor.h"


#define PHT_CTR_MAX  7//3
//chuan: for tournament predictor
#define TOURNAMENT_CTR_MAX 3
#define PHT_CTR_INIT 4//2
#define PHT_CTR_BAR  2

#define HIST_LEN   17
#define TOUR_LEN   16
#define BHT_BIT_SIZE 11
#define BHT_HIST_LENGTH 17
#define PHT_LOCAL_CTR_INIT 4//2
#define PHT_LOCAL_CTR_MAX  7//3
#define PHT_LOCAL_CTR_BAR  2
#define UINT16      unsigned short int

// loop prediction
#define LOOP_THRESHOLD  7
#define GREEDY  1

/////////////// STORAGE BUDGET JUSTIFICATION ////////////////
// Total storage budget: 52KB + 32 bits

// Total PHT counters for Global predictor: 2^16
// Total PHT size for global predictor = 2^16 * 2 bits/counter = 2^17 bits = 16KB
// GHR size for global predictor: 32 bits

// Total PHT counters for local predictor: 2^16
// Total PHT size for local predictor = 2^16 * 2 bits/counter = 2^17 bits = 16KB
// Total BHT size for local predictor = 2^11 * 16 bits/counter = 2^15 bits = 4KB
// Total Size for local predictor = 16KB + 4KB = 20KB

// Total Tournament counters is: 2^16
// Total Tournament counter's size = 2^16 * 2 bits/counter = 2^17 bits = 16KB
/////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

PREDICTOR::PREDICTOR(void){

  historyLength    = HIST_LEN;
  ghr              = 0;
  numPhtEntries    = (1<< HIST_LEN);

  pht = new UINT32[numPhtEntries];


  for(UINT32 ii=0; ii< numPhtEntries; ii++){
    pht[ii]=PHT_CTR_INIT;
  }

  //when 00, 01, use global predictor; when 10, 11, use local predictor
  numTournamentCounter = (1<<TOUR_LEN);
  predictorChooseCounter = new UINT32[numTournamentCounter];
  for(UINT32 jj=0; jj< numTournamentCounter; jj++){
    predictorChooseCounter[jj] = 0;
  }

  //Initialization for local branch predictor
  bht_history_length = BHT_HIST_LENGTH;
  bht_bit_size = BHT_BIT_SIZE;
  numBhtEntries    = (1<< bht_bit_size);
  bht = new UINT16[numBhtEntries];
  for(UINT32 kk=0; kk< numBhtEntries; kk++){
    bht[kk]=0;
  }

  numPhtLocalEntries = (1<<bht_history_length);
  pht_local = new UINT32[numPhtLocalEntries];
  for(UINT32 ll=0; ll< numPhtLocalEntries; ll++){
    pht_local[ll]=PHT_LOCAL_CTR_INIT;
  }

  // Initialization for loop predictor

  lct = new UINT32[numPhtEntries];
  lpt = new UINT32[numPhtEntries];

  for(UINT32 ii=0; ii< numPhtEntries; ii++){
    lct[ii]=0;
    lpt[ii]=0;
  }

}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool   PREDICTOR::GetPrediction(UINT32 PC){

  //Add for tournament predictor: when 00, 01, use global predictor; when 10, 11, use local predictor
  UINT32 pCC   = PC >> (32-TOUR_LEN);
  if (predictorChooseCounter[pCC] < TOURNAMENT_CTR_MAX/2+1) {
        //use global predictor
      return GetGlobalPrediction(PC);
  } else {
      //use local predictor
      return GetLocalPrediction(PC);
  }
}


//for global predictor
bool   PREDICTOR::GetGlobalPrediction(UINT32 PC){
    UINT32 phtIndex   = (PC^ghr) % (numPhtEntries);
    UINT32 phtCounter = pht[phtIndex];
    if (lpt[phtIndex] >= LOOP_THRESHOLD) {
      if (lct[phtIndex] == lpt[phtIndex]) {
        return TAKEN;
      } else {
        return NOT_TAKEN;
      }
    } else {
      if(phtCounter > PHT_CTR_MAX/2){
          return TAKEN;
      }else{
          return NOT_TAKEN;
      }
    }
}

//for local predictor
bool   PREDICTOR::GetLocalPrediction(UINT32 PC){
    UINT32 bhtIndex   = (PC >> (32-bht_bit_size));
    UINT16 bht_result = bht[bhtIndex];
    UINT32 pht_local_index = (PC^(UINT32)(bht_result))% (numPhtLocalEntries);
    if(pht_local[pht_local_index] > PHT_LOCAL_CTR_MAX/2){
        return TAKEN;
    }else{
        return NOT_TAKEN;
    }
}

//my new state machine
UINT32  PREDICTOR::StateAfterZero(UINT32 x, UINT32 max){
  x = (x << 1);
  return x & max;
}

UINT32  PREDICTOR::StateAfterOne(UINT32 x, UINT32 max){
  x = (x << 1)+1;
  return x & max;
}

UINT32 SatIncrementGreedy(UINT32 x, UINT32 max)
{
  if (x==max/2) return x+max/2+1;
  if (x<max) return x+1;
  return x;
}

UINT32 SatDecrementGreedy(UINT32 x, UINT32 max)
{
  if(x==max/2+1) return 0;
  if(x>0) return x-1;
  return x;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  PREDICTOR::UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){

  UINT32 phtIndex   = (PC^ghr) % (numPhtEntries);
  UINT32 phtCounter = pht[phtIndex];

  // update the PHT for global predictor
  if(resolveDir == TAKEN){
    if (GREEDY == 1) {
      pht[phtIndex] = SatIncrementGreedy(phtCounter, PHT_CTR_MAX);
    } else {
      pht[phtIndex] = SatIncrement(phtCounter, PHT_CTR_MAX);
    }
    if (lpt[phtIndex] == 0) {
      lct[phtIndex] += 1;
    } else if (lpt[phtIndex] == lct[phtIndex]) {
      lpt[phtIndex] = 0;
    } else {
      lct[phtIndex] += 1;
    }
  }else{
    if (GREEDY == 1) {
      pht[phtIndex] = SatDecrementGreedy(phtCounter, PHT_CTR_MAX);
    } else {
      pht[phtIndex] = SatDecrement(phtCounter);
    }
    lpt[phtIndex] = lct[phtIndex];
    lct[phtIndex] = 0;
  }

  // update the GHR for global predictor
  ghr = (ghr << 1);

  if(resolveDir == TAKEN){
    ghr++;
  }

  // update the tournament counter
  bool global_pred_result = GetGlobalPrediction(PC);
  bool local_pred_result = GetLocalPrediction(PC);
  UINT32 pCC   = PC >> (32-TOUR_LEN);
  //currently global predictor is in using
  if (predictorChooseCounter[pCC] < (TOURNAMENT_CTR_MAX/2 + 1)) {
        //if global predictor predicts not correct and local predictor predicts correct, will add 1
        if (global_pred_result != predDir && local_pred_result == predDir) predictorChooseCounter[pCC]++;
        if (global_pred_result == predDir && local_pred_result != predDir) {
            if (predictorChooseCounter[pCC] >0) predictorChooseCounter[pCC]--;
        }
  } else {
      //currently local predictor is in using
      if (local_pred_result != predDir &&  global_pred_result == predDir) predictorChooseCounter[pCC]--;
      if (global_pred_result != predDir && local_pred_result == predDir) {
        if (predictorChooseCounter[pCC] < TOURNAMENT_CTR_MAX) predictorChooseCounter[pCC]++;
      }
  }

  //update the BHT and PHT for local branch predictor
  //update the PHT_LOCAL
  UINT32 bhtIndex   = (PC >> (32-bht_bit_size));
  UINT16 bht_result = bht[bhtIndex];
  UINT32 pht_local_index = (PC^(UINT32)(bht_result))% (numPhtLocalEntries);
  UINT32 pht_local_counter = pht_local[pht_local_index];
  if(resolveDir == TAKEN){
    if (GREEDY == 1) {
      pht_local[pht_local_index] = SatIncrementGreedy(pht_local_counter, PHT_LOCAL_CTR_MAX);
    } else {
      pht_local[pht_local_index] = SatIncrement(pht_local_counter, PHT_LOCAL_CTR_MAX);
    }
  }else{
    if (GREEDY == 1) {
      pht_local[pht_local_index] = SatDecrementGreedy(pht_local_counter, PHT_LOCAL_CTR_MAX);
    } else {
      pht_local[pht_local_index] = SatDecrement(pht_local_counter);
    }
    
  }

  //update the bht for local predictor
  bht[bhtIndex] = (bht[bhtIndex] << 1);
  if(resolveDir == TAKEN){
    bht[bhtIndex]++;
  }

}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void    PREDICTOR::TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget){

  // This function is called for instructions which are not
  // conditional branches, just in case someone decides to design
  // a predictor that uses information from such instructions.
  // We expect most contestants to leave this function untouched.

  return;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
