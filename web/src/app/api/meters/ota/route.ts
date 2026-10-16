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
      .update({ pending_ota: true })
      .eq('id', meter_id);

    if (error) {
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({ 
      success: true, 
      message: 'OTA update scheduled! The meter will download and install the new firmware on its next sync (within 2s).' 
    });
  } catch (err: any) {
    console.error('API /meters/ota error:', err);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
