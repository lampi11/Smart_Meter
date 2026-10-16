import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';

export async function POST(request: Request) {
  try {
    const { meter_id, polarity } = await request.json();
    if (!meter_id || !polarity) {
      return NextResponse.json({ error: 'Meter ID and polarity (LOW or HIGH) required' }, { status: 400 });
    }

    const cleanPolarity = polarity.toUpperCase() === 'HIGH' ? 'HIGH' : 'LOW';

    const { error } = await supabase
      .from('meters')
      .update({ relay_polarity: cleanPolarity })
      .eq('id', meter_id);

    if (error) {
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({ 
      success: true, 
      polarity: cleanPolarity, 
      message: `Relay polarity updated to Active-${cleanPolarity}!` 
    });
  } catch (error: any) {
    console.error('API /meters/polarity error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
