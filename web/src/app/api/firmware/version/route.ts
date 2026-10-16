import { NextResponse } from 'next/server';

export async function GET() {
  return NextResponse.json({
    version: "1.0.7",
    url: "https://smart-meter-tau.vercel.app/firmware.bin",
    release_date: "2026-10-17",
    notes: "LCD Relay Status display (R:ONL / R:ONH), Anti-Blackout Survival Mode, multi-pin driving"
  }, {
    headers: {
      'Cache-Control': 'no-store, max-age=0'
    }
  });
}
