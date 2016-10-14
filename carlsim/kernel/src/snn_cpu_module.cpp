/* * Copyright (c) 2015 Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * *********************************************************************************************** *
 * CARLsim
 * created by: (MDR) Micah Richert, (JN) Jayram M. Nageswaran
 * maintained by:
 * (MA) Mike Avery <averym@uci.edu>
 * (MB) Michael Beyeler <mbeyeler@uci.edu>,
 * (KDC) Kristofor Carlson <kdcarlso@uci.edu>
 * (TSC) Ting-Shuo Chou <tingshuc@uci.edu>
 *
 * CARLsim available from http://socsci.uci.edu/~jkrichma/CARLsim/
 * Ver 5/22/2015
 */

#include <snn.h>

#include <spike_buffer.h>



// This method loops through all spikes that are generated by neurons with a delay of 1ms
// and delivers the spikes to the appropriate post-synaptic neuron
void SNN::doD1CurrentUpdate() {
	int k     = spikeCountD1Sec-1;
	int k_end = timeTableD1[simTimeMs + glbNetworkConfig.maxDelay];

	while((k>=k_end) && (k>=0)) {

		int neuron_id = managerRuntimeData.firingTableD1[k];
		assert(neuron_id < glbNetworkConfig.numN);

		DelayInfo dPar = managerRuntimeData.postDelayInfo[neuron_id*(glbNetworkConfig.maxDelay + 1)];

		unsigned int  offset = managerRuntimeData.cumulativePost[neuron_id];

		for(int idx_d = dPar.delay_index_start;
			idx_d < (dPar.delay_index_start + dPar.delay_length);
			idx_d = idx_d+1) {
				generatePostSpike( neuron_id, idx_d, offset, 0);
		}
		k=k-1;
	}
}

// This method loops through all spikes that are generated by neurons with a delay of 2+ms
// and delivers the spikes to the appropriate post-synaptic neuron
void SNN::doD2CurrentUpdate() {
	int k = spikeCountD2Sec-1;
	int k_end = timeTableD2[simTimeMs+1];
	int t_pos = simTimeMs;

	while((k>=k_end)&& (k >=0)) {

		// get the neuron id from the index k
		int i  = managerRuntimeData.firingTableD2[k];

		// find the time of firing from the timeTable using index k
		while (!((k >= timeTableD2[t_pos+glbNetworkConfig.maxDelay])&&(k < timeTableD2[t_pos+glbNetworkConfig.maxDelay+1]))) {
			t_pos = t_pos - 1;
			assert((t_pos+glbNetworkConfig.maxDelay-1)>=0);
		}

		// \TODO: Instead of using the complex timeTable, can neuronFiringTime value...???
		// Calculate the time difference between time of firing of neuron and the current time...
		int tD = simTimeMs - t_pos;

		assert((tD<glbNetworkConfig.maxDelay)&&(tD>=0));
		assert(i < glbNetworkConfig.numN);

		DelayInfo dPar = managerRuntimeData.postDelayInfo[i*(glbNetworkConfig.maxDelay+1)+tD];

		unsigned int offset = managerRuntimeData.cumulativePost[i];

		// for each delay variables
		for(int idx_d = dPar.delay_index_start;
			idx_d < (dPar.delay_index_start + dPar.delay_length);
			idx_d = idx_d+1) {
			generatePostSpike( i, idx_d, offset, tD);
		}

		k=k-1;
	}
}

void SNN::doSnnSim() {
	// decay STP vars and conductances
	doSTPUpdateAndDecayCond();

	updateSpikeGenerators();

	//generate all the scheduled spikes from the spikeBuffer..
	generateSpikes();

	// find the neurons that has fired..
	findFiring();

	timeTableD2[simTimeMs+glbNetworkConfig.maxDelay+1] = spikeCountD2Sec;
	timeTableD1[simTimeMs+glbNetworkConfig.maxDelay+1] = spikeCountD1Sec;

	doD2CurrentUpdate();
	doD1CurrentUpdate();
	globalStateUpdate();

	return;
}

// FIXME: wrong to use groupConfigs[0]
void SNN::doSTPUpdateAndDecayCond() {
	int spikeBufferFull = 0;

	//decay the STP variables before adding new spikes.
	for(int g=0; (g < numGroups) & !spikeBufferFull; g++) {
		for(int i=groupConfigs[0][g].lStartN; i<=groupConfigs[0][g].lEndN; i++) {
	   		//decay the STP variables before adding new spikes.
			if (groupConfigs[0][g].WithSTP) {
				int ind_plus  = STP_BUF_POS(i, simTime, glbNetworkConfig.maxDelay);
				int ind_minus = STP_BUF_POS(i, (simTime-1), glbNetworkConfig.maxDelay);
				managerRuntimeData.stpu[ind_plus] = managerRuntimeData.stpu[ind_minus]*(1.0-groupConfigs[0][g].STP_tau_u_inv);
				managerRuntimeData.stpx[ind_plus] = managerRuntimeData.stpx[ind_minus] + (1.0-managerRuntimeData.stpx[ind_minus])*groupConfigs[0][g].STP_tau_x_inv;
			}

			if (groupConfigs[0][g].Type&POISSON_NEURON)
				continue;

			// decay conductances
			if (sim_with_conductances) {
				managerRuntimeData.gAMPA[i]  *= dAMPA;
				managerRuntimeData.gGABAa[i] *= dGABAa;

				if (sim_with_NMDA_rise) {
					managerRuntimeData.gNMDA_r[i] *= rNMDA;	// rise
					managerRuntimeData.gNMDA_d[i] *= dNMDA;	// decay
				} else {
					managerRuntimeData.gNMDA[i]   *= dNMDA;	// instantaneous rise
				}

				if (sim_with_GABAb_rise) {
					managerRuntimeData.gGABAb_r[i] *= rGABAb;	// rise
					managerRuntimeData.gGABAb_d[i] *= dGABAb;	// decay
				} else {
					managerRuntimeData.gGABAb[i] *= dGABAb;	// instantaneous rise
				}
			}
			else {
				managerRuntimeData.current[i] = 0.0f; // in CUBA mode, reset current to 0 at each time step and sum up all wts
			}
		}
	}
}

// FIXME: wrong to use groupConfigs[0]
void SNN::findFiring() {
	int spikeBufferFull = 0;

	for(int g=0; (g < numGroups) & !spikeBufferFull; g++) {
		// given group of neurons belong to the poisson group....
		if (groupConfigs[0][g].Type&POISSON_NEURON)
			continue;

		// his flag is set if with_stdp is set and also grpType is set to have GROUP_SYN_FIXED
		for(int i=groupConfigs[0][g].lStartN; i <= groupConfigs[0][g].lEndN; i++) {

			assert(i < glbNetworkConfig.numNReg);

			if (managerRuntimeData.voltage[i] >= 30.0) {
				managerRuntimeData.voltage[i] = managerRuntimeData.Izh_c[i];
				managerRuntimeData.recovery[i] += managerRuntimeData.Izh_d[i];

				spikeBufferFull = addSpikeToTable(i, g);

				if (spikeBufferFull)
					break;

				// STDP calculation: the post-synaptic neuron fires after the arrival of a pre-synaptic spike
				if (!sim_in_testing && groupConfigs[0][g].WithSTDP) {
					unsigned int pos_ij = managerRuntimeData.cumulativePre[i]; // the index of pre-synaptic neuron
					for(int j=0; j < managerRuntimeData.Npre_plastic[i]; pos_ij++, j++) {
						int stdp_tDiff = (simTime-managerRuntimeData.synSpikeTime[pos_ij]);
						assert(!((stdp_tDiff < 0) && (managerRuntimeData.synSpikeTime[pos_ij] != MAX_SIMULATION_TIME)));

						if (stdp_tDiff > 0) {
							// check this is an excitatory or inhibitory synapse
							if (groupConfigs[0][g].WithESTDP && managerRuntimeData.maxSynWt[pos_ij] >= 0) { // excitatory synapse
								// Handle E-STDP curve
								switch (groupConfigs[0][g].WithESTDPcurve) {
								case EXP_CURVE: // exponential curve
									if (stdp_tDiff * groupConfigs[0][g].TAU_PLUS_INV_EXC < 25)
										managerRuntimeData.wtChange[pos_ij] += STDP(stdp_tDiff, groupConfigs[0][g].ALPHA_PLUS_EXC, groupConfigs[0][g].TAU_PLUS_INV_EXC);
									break;
								case TIMING_BASED_CURVE: // sc curve
									if (stdp_tDiff * groupConfigs[0][g].TAU_PLUS_INV_EXC < 25) {
										if (stdp_tDiff <= groupConfigs[0][g].GAMMA)
											managerRuntimeData.wtChange[pos_ij] += groupConfigs[0][g].OMEGA + groupConfigs[0][g].KAPPA * STDP(stdp_tDiff, groupConfigs[0][g].ALPHA_PLUS_EXC, groupConfigs[0][g].TAU_PLUS_INV_EXC);
										else // stdp_tDiff > GAMMA
											managerRuntimeData.wtChange[pos_ij] -= STDP(stdp_tDiff, groupConfigs[0][g].ALPHA_PLUS_EXC, groupConfigs[0][g].TAU_PLUS_INV_EXC);
									}
									break;
								default:
									KERNEL_ERROR("Invalid E-STDP curve!");
									break;
								}
							} else if (groupConfigs[0][g].WithISTDP && managerRuntimeData.maxSynWt[pos_ij] < 0) { // inhibitory synapse
								// Handle I-STDP curve
								switch (groupConfigs[0][g].WithISTDPcurve) {
								case EXP_CURVE: // exponential curve
									if (stdp_tDiff * groupConfigs[0][g].TAU_PLUS_INV_INB < 25) { // LTP of inhibitory synapse, which decreases synapse weight
										managerRuntimeData.wtChange[pos_ij] -= STDP(stdp_tDiff, groupConfigs[0][g].ALPHA_PLUS_INB, groupConfigs[0][g].TAU_PLUS_INV_INB);
									}
									break;
								case PULSE_CURVE: // pulse curve
									if (stdp_tDiff <= groupConfigs[0][g].LAMBDA) { // LTP of inhibitory synapse, which decreases synapse weight
										managerRuntimeData.wtChange[pos_ij] -= groupConfigs[0][g].BETA_LTP;
										//printf("I-STDP LTP\n");
									} else if (stdp_tDiff <= groupConfigs[0][g].DELTA) { // LTD of inhibitory syanpse, which increase sysnapse weight
										managerRuntimeData.wtChange[pos_ij] -= groupConfigs[0][g].BETA_LTD;
										//printf("I-STDP LTD\n");
									} else { /*do nothing*/}
									break;
								default:
									KERNEL_ERROR("Invalid I-STDP curve!");
									break;
								}
							}
						}
					}
				}
				spikeCountSec++;
			}
		}
	}
}

void SNN::generatePostSpike(unsigned int pre_i, unsigned int idx_d, unsigned int offset, int tD) {
	// get synaptic info...
	SynInfo post_info = managerRuntimeData.postSynapticIds[offset + idx_d];

	// get post-neuron id
	unsigned int post_i = GET_CONN_NEURON_ID(post_info);
	assert(post_i < glbNetworkConfig.numN);

	// get syn id
	int s_i = GET_CONN_SYN_ID(post_info);
	assert(s_i<(managerRuntimeData.Npre[post_i]));

	// get the cumulative position for quick access
	unsigned int pos_i = managerRuntimeData.cumulativePre[post_i] + s_i;
	assert(post_i < glbNetworkConfig.numNReg); // \FIXME is this assert supposed to be for pos_i?

	// get group id of pre- / post-neuron
	short int post_grpId = managerRuntimeData.grpIds[post_i];
	short int pre_grpId = managerRuntimeData.grpIds[pre_i];

	unsigned int pre_type = groupConfigs[0][pre_grpId].Type;

	// get connect info from the cumulative synapse index for mulSynFast/mulSynSlow (requires less memory than storing
	// mulSynFast/Slow per synapse or storing a pointer to grpConnectInfo_s)
	// mulSynFast will be applied to fast currents (either AMPA or GABAa)
	// mulSynSlow will be applied to slow currents (either NMDA or GABAb)
	short int mulIndex = managerRuntimeData.connIdsPreIdx[pos_i];
	assert(mulIndex>=0 && mulIndex<numConnections);


	// for each presynaptic spike, postsynaptic (synaptic) current is going to increase by some amplitude (change)
	// generally speaking, this amplitude is the weight; but it can be modulated by STP
	float change = managerRuntimeData.wt[pos_i];

	if (groupConfigs[0][pre_grpId].WithSTP) {
		// if pre-group has STP enabled, we need to modulate the weight
		// NOTE: Order is important! (Tsodyks & Markram, 1998; Mongillo, Barak, & Tsodyks, 2008)
		// use u^+ (value right after spike-update) but x^- (value right before spike-update)

		// dI/dt = -I/tau_S + A * u^+ * x^- * \delta(t-t_{spk})
		// I noticed that for connect(.., RangeDelay(1), ..) tD will be 0
		int ind_minus = STP_BUF_POS(pre_i, (simTime-tD-1), glbNetworkConfig.maxDelay);
		int ind_plus  = STP_BUF_POS(pre_i, (simTime-tD), glbNetworkConfig.maxDelay);

		change *= groupConfigs[0][pre_grpId].STP_A*managerRuntimeData.stpu[ind_plus]*managerRuntimeData.stpx[ind_minus];

//		fprintf(stderr,"%d: %d[%d], numN=%d, td=%d, maxDelay_=%d, ind-=%d, ind+=%d, stpu=[%f,%f], stpx=[%f,%f], change=%f, wt=%f\n",
//			simTime, pre_grpId, pre_i,
//					numN, tD, maxDelay_, ind_minus, ind_plus,
//					stpu[ind_minus], stpu[ind_plus], stpx[ind_minus], stpx[ind_plus], change, wt[pos_i]);
	}

	// update currents
	// NOTE: it's faster to += 0.0 rather than checking for zero and not updating
	if (sim_with_conductances) {
		if (pre_type & TARGET_AMPA) // if post_i expresses AMPAR
			managerRuntimeData.gAMPA [post_i] += change*mulSynFast[mulIndex]; // scale by some factor
		if (pre_type & TARGET_NMDA) {
			if (sim_with_NMDA_rise) {
				managerRuntimeData.gNMDA_r[post_i] += change*sNMDA*mulSynSlow[mulIndex];
				managerRuntimeData.gNMDA_d[post_i] += change*sNMDA*mulSynSlow[mulIndex];
			} else {
				managerRuntimeData.gNMDA [post_i] += change*mulSynSlow[mulIndex];
			}
		}
		if (pre_type & TARGET_GABAa)
			managerRuntimeData.gGABAa[post_i] -= change*mulSynFast[mulIndex]; // wt should be negative for GABAa and GABAb
		if (pre_type & TARGET_GABAb) {
			if (sim_with_GABAb_rise) {
				managerRuntimeData.gGABAb_r[post_i] -= change*sGABAb*mulSynSlow[mulIndex];
				managerRuntimeData.gGABAb_d[post_i] -= change*sGABAb*mulSynSlow[mulIndex];
			} else {
				managerRuntimeData.gGABAb[post_i] -= change*mulSynSlow[mulIndex];
			}
		}
	} else {
		managerRuntimeData.current[post_i] += change;
	}

	managerRuntimeData.synSpikeTime[pos_i] = simTime;

	// Got one spike from dopaminergic neuron, increase dopamine concentration in the target area
	if (pre_type & TARGET_DA) {
		managerRuntimeData.grpDA[post_grpId] += 0.04;
	}

	// STDP calculation: the post-synaptic neuron fires before the arrival of a pre-synaptic spike
	if (!sim_in_testing && groupConfigs[0][post_grpId].WithSTDP) {
		int stdp_tDiff = (simTime-managerRuntimeData.lastSpikeTime[post_i]);

		if (stdp_tDiff >= 0) {
			if (groupConfigs[0][post_grpId].WithISTDP && ((pre_type & TARGET_GABAa) || (pre_type & TARGET_GABAb))) { // inhibitory syanpse
				// Handle I-STDP curve
				switch (groupConfigs[0][post_grpId].WithISTDPcurve) {
				case EXP_CURVE: // exponential curve
					if ((stdp_tDiff*groupConfigs[0][post_grpId].TAU_MINUS_INV_INB)<25) { // LTD of inhibitory syanpse, which increase synapse weight
						managerRuntimeData.wtChange[pos_i] -= STDP(stdp_tDiff, groupConfigs[0][post_grpId].ALPHA_MINUS_INB, groupConfigs[0][post_grpId].TAU_MINUS_INV_INB);
					}
					break;
				case PULSE_CURVE: // pulse curve
					if (stdp_tDiff <= groupConfigs[0][post_grpId].LAMBDA) { // LTP of inhibitory synapse, which decreases synapse weight
						managerRuntimeData.wtChange[pos_i] -= groupConfigs[0][post_grpId].BETA_LTP;
					} else if (stdp_tDiff <= groupConfigs[0][post_grpId].DELTA) { // LTD of inhibitory syanpse, which increase synapse weight
						managerRuntimeData.wtChange[pos_i] -= groupConfigs[0][post_grpId].BETA_LTD;
					} else { /*do nothing*/ }
					break;
				default:
					KERNEL_ERROR("Invalid I-STDP curve");
					break;
				}
			} else if (groupConfigs[0][post_grpId].WithESTDP && ((pre_type & TARGET_AMPA) || (pre_type & TARGET_NMDA))) { // excitatory synapse
				// Handle E-STDP curve
				switch (groupConfigs[0][post_grpId].WithESTDPcurve) {
				case EXP_CURVE: // exponential curve
				case TIMING_BASED_CURVE: // sc curve
					if (stdp_tDiff * groupConfigs[0][post_grpId].TAU_MINUS_INV_EXC < 25)
						managerRuntimeData.wtChange[pos_i] += STDP(stdp_tDiff, groupConfigs[0][post_grpId].ALPHA_MINUS_EXC, groupConfigs[0][post_grpId].TAU_MINUS_INV_EXC);
					break;
				default:
					KERNEL_ERROR("Invalid E-STDP curve");
					break;
				}
			} else { /*do nothing*/ }
		}
		assert(!((stdp_tDiff < 0) && (managerRuntimeData.lastSpikeTime[post_i] != MAX_SIMULATION_TIME)));
	}
}

void SNN::generateSpikes() {
	SpikeBuffer::SpikeIterator spikeBufIter;
	SpikeBuffer::SpikeIterator spikeBufIterEnd = spikeBuf->back();

	for (spikeBufIter=spikeBuf->front(); spikeBufIter!=spikeBufIterEnd; ++spikeBufIter) {
		// get the neuron id for this particular spike
		int nid	 = spikeBufIter->neurId;

		// find associated group ID
		short int g = managerRuntimeData.grpIds[nid];

		// add spike to scheduling table
		addSpikeToTable (nid, g);

		// keep track of number of spikes
		spikeCountSec++;
		nPoissonSpikes++;
	}

	// tell the spike buffer to advance to the next time step
	spikeBuf->step();
}

// FIXME: wrong to use groupConfigs[0]
void SNN::generateSpikesFromFuncPtr(int grpId) {
	// \FIXME this function is a mess
	bool done;
	SpikeGeneratorCore* spikeGenFunc = groupConfigMap[grpId].spikeGenFunc;
	int timeSlice = groupConfigMDMap[grpId].currTimeSlice;
	int currTime = simTime;
	int spikeCnt = 0;
	for(int i = groupConfigs[0][grpId].gStartN; i <= groupConfigs[0][grpId].gEndN; i++) {
		// start the time from the last time it spiked, that way we can ensure that the refractory period is maintained
		int nextTime = managerRuntimeData.lastSpikeTime[i];
		if (nextTime == MAX_SIMULATION_TIME)
			nextTime = 0;

		// the end of the valid time window is either the length of the scheduling time slice from now (because that
		// is the max of the allowed propagated buffer size) or simply the end of the simulation
		int endOfTimeWindow = MIN(currTime+timeSlice,simTimeRunStop);

		done = false;
		while (!done) {
			// generate the next spike time (nextSchedTime) from the nextSpikeTime callback
			int nextSchedTime = spikeGenFunc->nextSpikeTime(this, grpId, i - groupConfigs[0][grpId].gStartN, currTime, 
				nextTime, endOfTimeWindow);

			// the generated spike time is valid only if:
			// - it has not been scheduled before (nextSchedTime > nextTime)
			//    - but careful: we would drop spikes at t=0, because we cannot initialize nextTime to -1...
			// - it is within the scheduling time slice (nextSchedTime < endOfTimeWindow)
			// - it is not in the past (nextSchedTime >= currTime)
			if ((nextSchedTime==0 || nextSchedTime>nextTime) && nextSchedTime<endOfTimeWindow && nextSchedTime>=currTime) {
//				fprintf(stderr,"%u: spike scheduled for %d at %u\n",currTime, i-groupConfigs[0][grpId].StartN,nextSchedTime);
				// scheduled spike...
				// \TODO CPU mode does not check whether the same AER event has been scheduled before (bug #212)
				// check how GPU mode does it, then do the same here.
				nextTime = nextSchedTime;
				spikeBuf->schedule(i, nextTime - currTime);
				spikeCnt++;
			} else {
				done = true;
			}
		}
	}
}

// FIXME: wrong to use groupConfigs[0]
void SNN::generateSpikesFromRate(int gGrpId) {
	bool done;
	PoissonRate* rate = groupConfigMDMap[gGrpId].ratePtr;
	float refPeriod = groupConfigMDMap[gGrpId].refractPeriod;
	int timeSlice   = groupConfigMDMap[gGrpId].currTimeSlice;
	int currTime = simTime;
	int spikeCnt = 0;

	if (rate == NULL)
		return;

	if (rate->isOnGPU()) {
		KERNEL_ERROR("Specifying rates on the GPU but using the CPU SNN is not supported.");
		exitSimulation(1);
	}

	const int nNeur = rate->getNumNeurons();
	if (nNeur != groupConfigMap[gGrpId].numN) {
		KERNEL_ERROR("Length of PoissonRate array (%d) did not match number of neurons (%d) for group %d(%s).",
			nNeur, groupConfigMap[gGrpId].numN, gGrpId, groupConfigMap[gGrpId].grpName.c_str());
		exitSimulation(1);
	}

	for (int neurId=0; neurId<nNeur; neurId++) {
		float frate = rate->getRate(neurId);

		// start the time from the last time it spiked, that way we can ensure that the refractory period is maintained
		int nextTime = managerRuntimeData.lastSpikeTime[groupConfigMDMap[gGrpId].gStartN + neurId];
		if (nextTime == MAX_SIMULATION_TIME)
			nextTime = 0;

		done = false;
		while (!done && frate>0) {
			nextTime = poissonSpike(nextTime, frate/1000.0, refPeriod);
			// found a valid timeSlice
			if (nextTime < (currTime+timeSlice)) {
				if (nextTime >= currTime) {
//					int nid = groupConfigs[0][grpId].StartN+cnt;
					spikeBuf->schedule(groupConfigMDMap[gGrpId].gStartN + neurId, nextTime-currTime);
					spikeCnt++;
				}
			}
			else {
				done=true;
			}
		}
	}
}

// FIXME: wrong to use groupConfigs[0]
void  SNN::globalStateUpdate() {
	double tmp_iNMDA, tmp_I;
	double tmp_gNMDA, tmp_gGABAb;

	for(int g = 0; g < numGroups; g++) {
		if (groupConfigs[0][g].Type & POISSON_NEURON) {
			if (groupConfigs[0][g].WithHomeostasis) {
				for(int i=groupConfigs[0][g].gStartN; i <= groupConfigs[0][g].gEndN; i++)
					managerRuntimeData.avgFiring[i] *= groupConfigs[0][g].avgTimeScale_decay;
			}
			continue;
		}

		// decay dopamine concentration
		if ((groupConfigs[0][g].WithESTDPtype == DA_MOD || groupConfigs[0][g].WithISTDP == DA_MOD) && managerRuntimeData.grpDA[g] > groupConfigs[0][g].baseDP) {
			managerRuntimeData.grpDA[g] *= groupConfigs[0][g].decayDP;
		}
		managerRuntimeData.grpDABuffer[g * 1000 + simTimeMs] = managerRuntimeData.grpDA[g];

		for(int i=groupConfigs[0][g].gStartN; i <= groupConfigs[0][g].gEndN; i++) {
			assert(i < glbNetworkConfig.numNReg);
			// update average firing rate for homeostasis
			if (groupConfigs[0][g].WithHomeostasis)
				managerRuntimeData.avgFiring[i] *= groupConfigs[0][g].avgTimeScale_decay;

			// update conductances
			if (sim_with_conductances) {
				// COBA model

				// all the tmpIs will be summed into current[i] in the following loop
				managerRuntimeData.current[i] = 0.0f;

				// \FIXME: these tmp vars cause a lot of rounding errors... consider rewriting
				for (int j=0; j<COND_INTEGRATION_SCALE; j++) {
					tmp_iNMDA = (managerRuntimeData.voltage[i]+80.0)*(managerRuntimeData.voltage[i]+80.0)/60.0/60.0;

					tmp_gNMDA = sim_with_NMDA_rise ? managerRuntimeData.gNMDA_d[i]-managerRuntimeData.gNMDA_r[i] : managerRuntimeData.gNMDA[i];
					tmp_gGABAb = sim_with_GABAb_rise ? managerRuntimeData.gGABAb_d[i]-managerRuntimeData.gGABAb_r[i] : managerRuntimeData.gGABAb[i];

					tmp_I = -(   managerRuntimeData.gAMPA[i]*(managerRuntimeData.voltage[i]-0)
									 + tmp_gNMDA*tmp_iNMDA/(1+tmp_iNMDA)*(managerRuntimeData.voltage[i]-0)
									 + managerRuntimeData.gGABAa[i]*(managerRuntimeData.voltage[i]+70)
									 + tmp_gGABAb*(managerRuntimeData.voltage[i]+90)
								   );

					managerRuntimeData.voltage[i] += ((0.04*managerRuntimeData.voltage[i]+5.0)*managerRuntimeData.voltage[i]+140.0-managerRuntimeData.recovery[i]+tmp_I+managerRuntimeData.extCurrent[i])
						/ COND_INTEGRATION_SCALE;
					assert(!isnan(managerRuntimeData.voltage[i]) && !isinf(managerRuntimeData.voltage[i]));

					// keep track of total current
					managerRuntimeData.current[i] += tmp_I;

					if (managerRuntimeData.voltage[i] > 30) {
						managerRuntimeData.voltage[i] = 30;
						j=COND_INTEGRATION_SCALE; // break the loop but evaluate u[i]
//						if (gNMDA[i]>=10.0f) KERNEL_WARN("High NMDA conductance (gNMDA>=10.0) may cause instability");
//						if (gGABAb[i]>=2.0f) KERNEL_WARN("High GABAb conductance (gGABAb>=2.0) may cause instability");
					}
					if (managerRuntimeData.voltage[i] < -90)
						managerRuntimeData.voltage[i] = -90;
					managerRuntimeData.recovery[i]+=managerRuntimeData.Izh_a[i]*(managerRuntimeData.Izh_b[i]*managerRuntimeData.voltage[i]-managerRuntimeData.recovery[i])/COND_INTEGRATION_SCALE;
				} // end COND_INTEGRATION_SCALE loop
			} else {
				// CUBA model
				managerRuntimeData.voltage[i] += 0.5*((0.04*managerRuntimeData.voltage[i]+5.0)*managerRuntimeData.voltage[i] + 140.0 - managerRuntimeData.recovery[i]
					+ managerRuntimeData.current[i] + managerRuntimeData.extCurrent[i]); //for numerical stability
				managerRuntimeData.voltage[i] += 0.5*((0.04*managerRuntimeData.voltage[i]+5.0)*managerRuntimeData.voltage[i] + 140.0 - managerRuntimeData.recovery[i]
					+ managerRuntimeData.current[i] + managerRuntimeData.extCurrent[i]); //time step is 0.5 ms
				if (managerRuntimeData.voltage[i] > 30)
					managerRuntimeData.voltage[i] = 30;
				if (managerRuntimeData.voltage[i] < -90)
					managerRuntimeData.voltage[i] = -90;
				managerRuntimeData.recovery[i]+=managerRuntimeData.Izh_a[i]*(managerRuntimeData.Izh_b[i]*managerRuntimeData.voltage[i]-managerRuntimeData.recovery[i]);
			} // end COBA/CUBA
		} // end StartN...EndN
	} // end numGroups
}

// FIXME: wrong to use groupConfigs[0]
// This function updates the synaptic weights from its derivatives..
void SNN::updateWeights() {
	// at this point we have already checked for sim_in_testing and sim_with_fixedwts
	assert(sim_in_testing==false);
	assert(sim_with_fixedwts==false);

	// update synaptic weights here for all the neurons..
	for(int g = 0; g < numGroups; g++) {
		// no changable weights so continue without changing..
		if(groupConfigs[0][g].FixedInputWts || !(groupConfigs[0][g].WithSTDP))
			continue;

		for(int i = groupConfigs[0][g].gStartN; i <= groupConfigs[0][g].gEndN; i++) {
			assert(i < glbNetworkConfig.numNReg);
			unsigned int offset = managerRuntimeData.cumulativePre[i];
			float diff_firing = 0.0;
			float homeostasisScale = 1.0;

			if(groupConfigs[0][g].WithHomeostasis) {
				assert(managerRuntimeData.baseFiring[i]>0);
				diff_firing = 1-managerRuntimeData.avgFiring[i]/managerRuntimeData.baseFiring[i];
				homeostasisScale = groupConfigs[0][g].homeostasisScale;
			}

			if (i==groupConfigs[0][g].gStartN)
				KERNEL_DEBUG("Weights, Change at %d (diff_firing: %f)", simTimeSec, diff_firing);

			for(int j = 0; j < managerRuntimeData.Npre_plastic[i]; j++) {
				//	if (i==groupConfigs[0][g].StartN)
				//		KERNEL_DEBUG("%1.2f %1.2f \t", wt[offset+j]*10, wtChange[offset+j]*10);
				float effectiveWtChange = stdpScaleFactor_ * managerRuntimeData.wtChange[offset + j];
//				if (wtChange[offset+j])
//					printf("connId=%d, wtChange[%d]=%f\n",connIdsPreIdx[offset+j],offset+j,wtChange[offset+j]);

				// homeostatic weight update
				// FIXME: check WithESTDPtype and WithISTDPtype first and then do weight change update
				switch (groupConfigs[0][g].WithESTDPtype) {
				case STANDARD:
					if (groupConfigs[0][g].WithHomeostasis) {
						managerRuntimeData.wt[offset+j] += (diff_firing*managerRuntimeData.wt[offset+j]*homeostasisScale + managerRuntimeData.wtChange[offset+j])*managerRuntimeData.baseFiring[i]/groupConfigs[0][g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						// just STDP weight update
						managerRuntimeData.wt[offset+j] += effectiveWtChange;
					}
					break;
				case DA_MOD:
					if (groupConfigs[0][g].WithHomeostasis) {
						effectiveWtChange = managerRuntimeData.grpDA[g] * effectiveWtChange;
						managerRuntimeData.wt[offset+j] += (diff_firing*managerRuntimeData.wt[offset+j]*homeostasisScale + effectiveWtChange)*managerRuntimeData.baseFiring[i]/groupConfigs[0][g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						managerRuntimeData.wt[offset+j] += managerRuntimeData.grpDA[g] * effectiveWtChange;
					}
					break;
				case UNKNOWN_STDP:
				default:
					// we shouldn't even be in here if !WithSTDP
					break;
				}

				switch (groupConfigs[0][g].WithISTDPtype) {
				case STANDARD:
					if (groupConfigs[0][g].WithHomeostasis) {
						managerRuntimeData.wt[offset+j] += (diff_firing*managerRuntimeData.wt[offset+j]*homeostasisScale + managerRuntimeData.wtChange[offset+j])*managerRuntimeData.baseFiring[i]/groupConfigs[0][g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						// just STDP weight update
						managerRuntimeData.wt[offset+j] += effectiveWtChange;
					}
					break;
				case DA_MOD:
					if (groupConfigs[0][g].WithHomeostasis) {
						effectiveWtChange = managerRuntimeData.grpDA[g] * effectiveWtChange;
						managerRuntimeData.wt[offset+j] += (diff_firing*managerRuntimeData.wt[offset+j]*homeostasisScale + effectiveWtChange)*managerRuntimeData.baseFiring[i]/groupConfigs[0][g].avgTimeScale/(1+fabs(diff_firing)*50);
					} else {
						managerRuntimeData.wt[offset+j] += managerRuntimeData.grpDA[g] * effectiveWtChange;
					}
					break;
				case UNKNOWN_STDP:
				default:
					// we shouldn't even be in here if !WithSTDP
					break;
				}

				// It is users' choice to decay weight change or not
				// see setWeightAndWeightChangeUpdate()
				managerRuntimeData.wtChange[offset+j] *= wtChangeDecay_;

				// if this is an excitatory or inhibitory synapse
				if (managerRuntimeData.maxSynWt[offset + j] >= 0) {
					if (managerRuntimeData.wt[offset + j] >= managerRuntimeData.maxSynWt[offset + j])
						managerRuntimeData.wt[offset + j] = managerRuntimeData.maxSynWt[offset + j];
					if (managerRuntimeData.wt[offset + j] < 0)
						managerRuntimeData.wt[offset + j] = 0.0;
				} else {
					if (managerRuntimeData.wt[offset + j] <= managerRuntimeData.maxSynWt[offset + j])
						managerRuntimeData.wt[offset + j] = managerRuntimeData.maxSynWt[offset + j];
					if (managerRuntimeData.wt[offset+j] > 0)
						managerRuntimeData.wt[offset+j] = 0.0;
				}
			}
		}
	}
}

/*!
 * \brief This function is called every second by SNN::runNetwork(). It updates the firingTableD1(D2) and
 * timeTableD1(D2) by removing older firing information.
 */
void SNN::shiftSpikeTables() {
	// Read the neuron ids that fired in the last glbNetworkConfig.maxDelay seconds
	// and put it to the beginning of the firing table...
	for(int p=timeTableD2[999],k=0;p<timeTableD2[999+glbNetworkConfig.maxDelay+1];p++,k++) {
		managerRuntimeData.firingTableD2[k]=managerRuntimeData.firingTableD2[p];
	}

	for(int i=0; i < glbNetworkConfig.maxDelay; i++) {
		timeTableD2[i+1] = timeTableD2[1000+i+1]-timeTableD2[1000];
	}

	timeTableD1[glbNetworkConfig.maxDelay] = 0;

	/* the code of weight update has been moved to SNN::updateWeights() */

	spikeCount	+= spikeCountSec;
	spikeCountD2 += (spikeCountD2Sec-timeTableD2[glbNetworkConfig.maxDelay]);
	spikeCountD1 += spikeCountD1Sec;

	spikeCountD1Sec  = 0;
	spikeCountSec = 0;
	spikeCountD2Sec = timeTableD2[glbNetworkConfig.maxDelay];
}

void SNN::allocateSNN_CPU(int netId) {
	managerRuntimeData.allocated = true;
	managerRuntimeData.memType = CPU_MODE;
}