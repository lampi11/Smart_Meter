import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';
import crypto from 'crypto';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const {
      device_id,
      fw_version,
      uptime_seconds,
      free_heap,
      wifi_rssi,
      voltage_avg,
      voltage_min,
      voltage_max,
      power_factor_avg,
      relay_cycle_count,
      error_count
    } = body;

    // Create a 1-way anonymized device hash so R&D can analyze trends across individual units without knowing identity/location
    const salt = 'lampitex_rnd_v1';
    const device_hash = crypto
      .createHash('sha256')
      .update((device_id || 'unknown') + salt)
      .digest('hex')
      .substring(0, 16);

    const { error } = await supabase.from('anonymous_telemetry').insert({
      device_hash,
      fw_version: fw_version || '1.0.0',
      uptime_seconds: uptime_seconds || 0,
      free_heap: free_heap || null,
      wifi_rssi: wifi_rssi || null,
      voltage_avg: voltage_avg || null,
      voltage_min: voltage_min || null,
      voltage_max: voltage_max || null,
      power_factor_avg: power_factor_avg || null,
      relay_cycle_count: relay_cycle_count || 0,
      error_count: error_count || 0
    });

    if (error) {
      console.error('Anonymous telemetry insert error:', error);
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({ success: true });
  } catch (err: any) {
    console.error('API /anonymous error:', err);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
