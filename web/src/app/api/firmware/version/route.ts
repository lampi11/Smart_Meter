import { NextResponse } from 'next/server';

export async function GET() {
  return NextResponse.json({
    version: "1.0.4",
    url: "https://smart-meter-tau.vercel.app/firmware.bin",
    release_date: "2026-10-17",
    notes: "Plausibility safeguards against corrupt energy jumps, direct balance set, and bounds validation"
  }, {
    headers: {
      'Cache-Control': 'no-store, max-age=0'
    }
  });
}
