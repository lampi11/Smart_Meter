import { NextResponse } from 'next/server';

export async function GET() {
  return NextResponse.json({
    version: "1.0.1",
    url: "https://smart-meter-tau.vercel.app/firmware.bin",
    release_date: "2026-10-16",
    notes: "Adaptive 64 kbps low-bandwidth telemetry, automatic cloud OTA updates, and live LCD progress"
  }, {
    headers: {
      'Cache-Control': 'no-store, max-age=0'
    }
  });
}
