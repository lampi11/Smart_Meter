import { NextResponse } from 'next/server';

export async function GET() {
  return NextResponse.json({
    version: "1.0.8",
    url: "https://smart-meter-tau.vercel.app/firmware.bin",
    release_date: "2026-10-24",
    notes: "Active-HIGH relay drive for SSR-40DA (Terminal 3 on GPIO 4, Terminal 4 on GND)"
  }, {
    headers: {
      'Cache-Control': 'no-store, max-age=0'
    }
  });
}
