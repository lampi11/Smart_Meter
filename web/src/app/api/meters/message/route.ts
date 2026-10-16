import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';

export async function POST(request: Request) {
  try {
    const { meter_id, message } = await request.json();

    if (!meter_id || !message) {
      return NextResponse.json({ error: 'Meter ID and message required' }, { status: 400 });
    }

    const trimmed = String(message).substring(0, 16); // LCD is 16 chars wide

    const { error } = await supabase
      .from('meters')
      .update({ pending_message: trimmed })
      .eq('id', meter_id);

    if (error) {
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({ success: true, message: trimmed });
  } catch (error: any) {
    console.error('API /meters/message error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
