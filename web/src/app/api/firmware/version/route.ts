import { NextResponse } from 'next/server';

export async function GET() {
  return NextResponse.json({
    version: "1.0.5",
    url: "https://smart-meter-tau.vercel.app/firmware.bin",
    release_date: "2026-10-17",
    notes: "Active-LOW SSR relay drive, unconditional power restore loop, dynamic polarity control"
  }, {
    headers: {
      'Cache-Control': 'no-store, max-age=0'
    }
  });
}
