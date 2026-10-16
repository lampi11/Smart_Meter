import { NextResponse } from 'next/server';

export async function GET() {
  return NextResponse.json({
    version: "1.0.3",
    url: "https://smart-meter-tau.vercel.app/firmware.bin",
    release_date: "2026-10-17",
    notes: "Reliable connection management, independent PZEM sensor reading, and NTP sync fix"
  }, {
    headers: {
      'Cache-Control': 'no-store, max-age=0'
    }
  });
}
