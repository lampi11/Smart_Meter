import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';

export async function POST(request: Request) {
  try {
    const data = await request.json();
    const { device_id, readings, balance_after, relay_on } = data;

    if (!device_id) {
      return NextResponse.json({ error: 'Device ID required' }, { status: 400 });
    }

    const { data: result, error } = await supabase.rpc('record_telemetry', {
      p_device_id: device_id,
      p_readings: readings || [],
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
      recharge: result?.recharge ?? 0
    });
  } catch (error: any) {
    console.error('API /telemetry error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
