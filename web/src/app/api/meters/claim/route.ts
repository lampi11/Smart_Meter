import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabaseClient';

export async function POST(request: Request) {
  try {
    const { serial_number, claim_code, label, owner_id, property_id } = await request.json();

    if (!serial_number || !claim_code) {
      return NextResponse.json({ error: 'Serial number and claim code are required' }, { status: 400 });
    }

    // 1. Look up meter in product_registry (flexible: handles with or without hyphens/spaces)
    const rawSerial = serial_number.trim().toUpperCase();
    const cleanInput = rawSerial.replace(/[-\s]/g, '');

    const { data: allProducts, error: prodErr } = await supabase
      .from('product_registry')
      .select('*');

    const product = (allProducts || []).find((p: any) => {
      const cleanDb = (p.serial_number || '').replace(/[-\s]/g, '').toUpperCase();
      return cleanDb === cleanInput || p.serial_number.toUpperCase() === rawSerial;
    });

    if (prodErr || !product) {
      return NextResponse.json({ 
        error: 'Serial number not found in manufacturer database. Please verify the serial number on the device.' 
      }, { status: 404 });
    }

    if (product.is_claimed) {
      return NextResponse.json({ 
        error: 'This meter has already been registered and claimed by an owner.' 
      }, { status: 409 });
    }

    if (product.claim_code !== claim_code.trim()) {
      return NextResponse.json({ 
        error: 'Invalid Security Claim Code. Check the 6-digit code shown on the device setup screen.' 
      }, { status: 403 });
    }

    // 2. Mark product as claimed
    await supabase
      .from('product_registry')
      .update({ is_claimed: true, claimed_at: new Date().toISOString() })
      .eq('id', product.id);

    // 3. Register meter under the owner
    const finalOwnerId = owner_id || '00000000-0000-0000-0000-000000000001';
    
    // Ensure owner row exists
    await supabase.from('owners').upsert({
      id: finalOwnerId,
      name: 'Owner',
      email: 'owner@lampitex.com'
    }, { onConflict: 'id' });

    const { data: newMeter, error: meterErr } = await supabase
      .from('meters')
      .insert({
        owner_id: finalOwnerId,
        property_id: property_id || null,
        serial_number: product.serial_number,
        device_id: product.device_mac || product.serial_number,
        label: label?.trim() || `Meter ${product.serial_number}`,
        balance: 100.00,
        tariff_grid: 8.00,
        relay_on: true
      })
      .select()
      .single();

    if (meterErr) {
      console.error('Meter registration error:', meterErr);
      return NextResponse.json({ error: meterErr.message }, { status: 500 });
    }

    return NextResponse.json({
      success: true,
      meter: newMeter
    });
  } catch (err: any) {
    console.error('API /meters/claim error:', err);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
