import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';

export async function POST(request: Request) {
  try {
    const { meter_id } = await request.json();
    if (!meter_id) {
      return NextResponse.json({ error: 'Meter ID required' }, { status: 400 });
    }

    const { error } = await supabase
      .from('meters')
      .update({ pending_wifi_reset: true })
      .eq('id', meter_id);

    if (error) {
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({ success: true, message: 'Wi-Fi reset signal queued for meter' });
  } catch (error: any) {
    console.error('API /meters/reset-wifi error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
