import { NextResponse } from 'next/server';

export async function GET() {
  return NextResponse.json({
    version: "1.0.2",
    url: "https://smart-meter-tau.vercel.app/firmware.bin",
    release_date: "2026-10-17",
    notes: "Remote Wi-Fi reset over-the-air, captive portal setup improvements, and OTA rollback safeguard"
  }, {
    headers: {
      'Cache-Control': 'no-store, max-age=0'
    }
  });
}
