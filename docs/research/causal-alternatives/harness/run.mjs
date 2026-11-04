// Local research harness. No browser, WASM, network, package scripts or installation.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { createDetector, parseWeights } from './incport.mjs';

const [configurationPath, outputPath] = process.argv.slice(2);
if (!configurationPath || !outputPath) throw new Error('Expected configuration JSON and output directory');
const config = JSON.parse(fs.readFileSync(configurationPath, 'utf8'));
fs.mkdirSync(outputPath, {recursive:true});
const params = {fast:{on:.7,off:.42},piano:{on:.6,off:.36},voice:{on:.65,off:.39}}[config.model];
if(!params) throw new Error('Invalid model');
const blob=fs.readFileSync(new URL(config.model+'.bin',import.meta.url));
if(blob.toString('ascii',0,4)!=='PPW1')throw new Error('Invalid model magic');
const headerLength=blob.readUInt32LE(4);
if(headerLength>100000||headerLength+8>blob.length)throw new Error('Invalid model header');
const header=JSON.parse(blob.toString('utf8',8,8+headerLength));
const start=(8+headerLength+3)&~3;
const data=Uint8Array.from(blob.subarray(start)).buffer;
for(const tensor of Object.values(header.tensors)){
  const count=tensor.shape.reduce((a,b)=>a*b,1);
  if(tensor.offset<0||(tensor.offset+count)*4>data.byteLength)throw new Error('Invalid tensor bounds');
}
const weights=parseWeights({tensors:header.tensors,const:header.const},data);

function wav(file){
  const bytes=fs.readFileSync(file);
  if(bytes.toString('ascii',0,4)!=='RIFF'||bytes.toString('ascii',8,12)!=='WAVE')throw new Error('Expected RIFF WAV');
  let format=null,audio=null;
  for(let at=12;at+8<=bytes.length;){
    const name=bytes.toString('ascii',at,at+4),length=bytes.readUInt32LE(at+4),first=at+8;
    if(first+length>bytes.length)throw new Error('Truncated WAV chunk');
    if(name==='fmt ')format={encoding:bytes.readUInt16LE(first),channels:bytes.readUInt16LE(first+2),rate:bytes.readUInt32LE(first+4),bits:bytes.readUInt16LE(first+14)};
    if(name==='data')audio=bytes.subarray(first,first+length);
    at=first+length+(length&1);
  }
  if(!format||!audio||format.channels!==1||format.rate!==48000)throw new Error('Expected mono 48 kHz fixtures');
  const width=format.bits/8,frames=audio.length/width;
  if(!Number.isInteger(frames)||![16,24,32].includes(format.bits))throw new Error('Unsupported WAV sample width');
  const samples=new Float32Array(frames);
  for(let n=0;n<frames;n++){
    let value;
    if(format.encoding===3&&format.bits===32)value=audio.readFloatLE(n*4);
    else if(format.encoding===1&&format.bits===16)value=audio.readInt16LE(n*2)/32768;
    else if(format.encoding===1&&format.bits===24)value=audio.readIntLE(n*3,3)/8388608;
    else if(format.encoding===1&&format.bits===32)value=audio.readInt32LE(n*4)/2147483648;
    else throw new Error('Unsupported WAV encoding');
    if(!Number.isFinite(value))throw new Error('Nonfinite fixture');
    samples[n]=value;
  }
  return samples;
}
function percentile(v,q){return v.length?[...v].sort((a,b)=>a-b)[Math.ceil(q*(v.length-1))]:null;}
const summaries=[];const block=config.block??64;
for(let index=0;index<config.inputs.length;index++){
  const item=config.inputs[index],samples=wav(item.file);
  const initStart=process.hrtime.bigint();const detector=createDetector(weights,{inputRate:48000});
  const initializationMs=Number(process.hrtime.bigint()-initStart)/1e6;
  const held=new Uint8Array(88),events=[],frames=[],times=[];
  const total=Math.ceil((samples.length+12000)/block)*block;
  let firstFrame=null,peakHeld=0;
  const cpuBefore=process.cpuUsage();const captureStart=process.hrtime.bigint();
  for(let offset=0;offset<total;offset+=block){
    const chunk=new Float32Array(block);chunk.set(samples.subarray(offset,Math.min(offset+block,samples.length)));
    const before=process.hrtime.bigint();const output=detector.push(chunk);times.push(Number(process.hrtime.bigint()-before)/1000);
    for(const frame of output){
      const emitted=Math.round(frame.emit*48000);
      if(emitted>offset+block||emitted<offset)throw new Error('Frame availability violates causal delivering block');
      if(firstFrame===null)firstFrame=emitted;
      const row=new Float32Array(90);row[0]=frame.t;row[1]=frame.emit;row.set(frame.probs,2);frames.push(Buffer.from(row.buffer));
      let active=0;
      for(let key=0;key<88;key++){
        const value=frame.probs[key];if(!Number.isFinite(value))throw new Error('Nonfinite model output');
        const next=held[key]?value>params.off:value>params.on;
        if(next!==Boolean(held[key]))events.push({sample:emitted,on:next?1:0,note:key+21,probability:value,feature_time:frame.t});
        held[key]=next?1:0;active+=held[key];
      }
      peakHeld=Math.max(peakHeld,active);
    }
  }
  const cpu=process.cpuUsage(cpuBefore),wallMs=Number(process.hrtime.bigint()-captureStart)/1e6;
  const caseOut=path.join(outputPath,String(index).padStart(3,'0'));
  fs.writeFileSync(caseOut+'-events.json',JSON.stringify(events)+'\n');fs.writeFileSync(caseOut+'-frames.f32',Buffer.concat(frames));
  summaries.push({id:item.id,file:item.file,source_frames:samples.length,processed_frames:total,model:config.model,block,threshold:params,initialization_ms:initializationMs,first_frame_sample:firstFrame,frame_count:frames.length,event_count:events.length,hung_after_silence:[...held].reduce((a,b)=>a+b,0),peak_held:peakHeld,wall_ms:wallMs,cpu_ms:(cpu.user+cpu.system)/1000,callback_us:{median:percentile(times,.5),p95:percentile(times,.95),p99:percentile(times,.99),maximum:Math.max(...times)},deadline_misses:times.filter(t=>t>block/48000*1e6).length,events_file:path.basename(caseOut)+'-events.json',frames_file:path.basename(caseOut)+'-frames.f32'});
  fs.writeFileSync(path.join(outputPath,'capture.json'),JSON.stringify({scope:'Private causal integer-note admission benchmark; not per-voice bends or musical acceptance',model:config.model,block,weights_sha256:crypto.createHash('sha256').update(blob).digest('hex'),inputs:summaries},null,2)+'\n');
  process.stdout.write(`${index+1}/${config.inputs.length} ${item.id}: ${events.length} events, ${(wallMs/1000).toFixed(3)}s wall\n`);
}
