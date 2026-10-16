import { NextResponse } from 'next/server';

export async function GET() {
  return NextResponse.json({
    version: "1.0.6",
    url: "https://smart-meter-tau.vercel.app/firmware.bin",
    release_date: "2026-10-17",
    notes: "Anti-Blackout Survival Mode (0.5A/110W throttle on low balance), Active-LOW SSR relay driving, clean TLS heap cleanup"
  }, {
    headers: {
      'Cache-Control': 'no-store, max-age=0'
    }
  });
}
