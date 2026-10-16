import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';

export async function POST(request: Request) {
  try {
    const data = await request.json();

    // Support both standard verbose keys and ultra-compact 64 kbps micro-keys
    const device_id = data.device_id || data.d;
    const balance_after = data.balance_after ?? data.b ?? null;
    const relay_on = data.relay_on ?? (data.r === 1 ? true : data.r === 0 ? false : null);
    const rawReadings = data.readings || data.rdgs || [];

    if (!device_id) {
      return NextResponse.json({ error: 'Device ID required' }, { status: 400 });
    }

    // Normalize readings array
    const readings = rawReadings.map((r: any) => ({
      timestamp: r.timestamp ?? r.t ?? Math.floor(Date.now() / 1000),
      voltage: r.voltage ?? r.v ?? 0,
      current: r.current ?? r.i ?? 0,
      power: r.power ?? r.p ?? 0,
      energy: r.energy ?? r.e ?? 0,
      balance_after: r.balance_after ?? r.b ?? balance_after,
      relay_on: r.relay_on ?? (r.r === 1 ? true : r.r === 0 ? false : relay_on)
    }));

    const { data: result, error } = await supabase.rpc('record_telemetry', {
      p_device_id: device_id,
      p_readings: readings,
      p_balance_after: balance_after ?? null,
      p_relay_on: relay_on ?? null
    });

    if (error) {
      console.error('RPC record_telemetry error:', error);
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({
      success: result?.success ?? true,
      message: result?.message ?? null,
      recharge: result?.recharge ?? 0,
      ota: result?.ota ?? false,
      reset_wifi: result?.reset_wifi ?? false,
      latest_version: "1.0.3",
      ota_url: "https://smart-meter-tau.vercel.app/firmware.bin"
    });
  } catch (error: any) {
    console.error('API /telemetry error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
